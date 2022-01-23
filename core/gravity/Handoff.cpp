#include "gravity/Handoff.h"
#include "gravity/Collision.h"
#include "objects/containers/FlatSet.h"
#include "objects/finders/KdTree.h"
#include "objects/utility/EnumMap.h"
#include "post/Analysis.h"
#include "quantities/Attractor.h"
#include "quantities/Quantity.h"
#include "sph/Materials.h"
#include "sph/kernel/Kernel.h"
#include "thread/ThreadLocal.h"
#include <iostream>

NAMESPACE_SPH_BEGIN

static RegisterEnum<HandoffRadius> sHandoffRadius({
    { HandoffRadius::EQUAL_VOLUME,
        "equal_volume",
        "Assume equal volume for solid spheres; r_solid = m / (4/3 pi rho_sph)^(1/3)." },
    { HandoffRadius::SMOOTHING_LENGTH,
        "smoothing_length",
        "Use a multiple of the smoothing length; r_solid = multiplier * h." },
});

Storage smoothedToSolidHandoff(const Storage& input, const HandoffParams& params) {
    // we don't need any material, so just pass some dummy
    Storage spheres(makeAuto<NullMaterial>(EMPTY_SETTINGS));

    // clone required quantities
    spheres.insert<Vector>(
        QuantityId::POSITION, OrderEnum::SECOND, input.getValue<Vector>(QuantityId::POSITION).clone());
    spheres.getDt<Vector>(QuantityId::POSITION) = input.getDt<Vector>(QuantityId::POSITION).clone();
    spheres.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, input.getValue<Float>(QuantityId::MASS).clone());

    // radii handoff
    ArrayView<const Float> m = input.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> rho = input.getValue<Float>(QuantityId::DENSITY);
    ArrayView<Vector> r_sphere = spheres.getValue<Vector>(QuantityId::POSITION);
    SPH_ASSERT(r_sphere.size() == rho.size());
    for (Size i = 0; i < r_sphere.size(); ++i) {
        switch (params.radiusType) {
        case (HandoffRadius::EQUAL_VOLUME):
            r_sphere[i][H] = cbrt(3._f * m[i] / (4._f * PI * rho[i]));
            break;
        case (HandoffRadius::SMOOTHING_LENGTH):
            r_sphere[i][H] = params.smoothingLengthMult * r_sphere[i][H];
            break;
        default:
            NOT_IMPLEMENTED;
        }
    }

    if (params.removeSublimated) {
        Array<Size> toRemove;
        ArrayView<const Float> u = input.getValue<Float>(QuantityId::ENERGY);
        for (Size matId = 0; matId < input.getMaterialCnt(); ++matId) {
            MaterialView mat = input.getMaterial(matId);
            const Float u_max = mat->getParam<Float>(BodySettingsId::TILLOTSON_SUBLIMATION);
            for (Size i : mat.sequence()) {
                if (u[i] > u_max) {
                    toRemove.push(i);
                }
            }
        }
        spheres.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED | Storage::IndicesFlag::PROPAGATE);
    }

    // copy attractors as-is
    for (const Attractor& a : input.getAttractors()) {
        spheres.addAttractor(a);
    }

    return spheres;
}

class InnerParticleChecker : public Post::IComponentChecker {
private:
    ArrayView<const uint8_t> surface;

public:
    explicit InnerParticleChecker(ArrayView<const uint8_t> surface)
        : surface(surface) {}

    virtual bool belong(const Size i, const Size j) const override {
        return !surface[i] && !surface[j];
    }
};

static Array<uint8_t> flagSurfaceParticles(IScheduler& scheduler,
    const IBasicFinder& finder,
    ArrayView<const Vector> r,
    const Float surfacenessThreshold) {
    Array<uint8_t> surface(r.size());
    surface.fill(0);

    ThreadLocal<Array<NeighborRecord>> neighsTl(scheduler);
    parallelFor(scheduler, neighsTl, 0, r.size(), [&](const Size i, Array<NeighborRecord>& neighs) {
        neighs.clear();
        finder.findAll(r[i], 2._f * r[i][H], neighs);
        Vector normal = Vector(0._f);
        Float weight = 0._f;
        for (const NeighborRecord& n : neighs) {
            const Size j = n.index;
            if (i == j || getSqrLength(r[i] - r[j]) < EPS) {
                continue;
            }
            const Float v = sphereVolume(r[j][H]);
            normal += v * getNormalized(r[j] - r[i]);
            weight += v;
        }
        if (weight > 0._f) {
            normal /= weight;
            surface[i] = getLength(normal) > surfacenessThreshold;
        }
    });
    return surface;
}

static void mergeComponent(ArrayView<Vector> r,
    ArrayView<const Size> indices,
    MergingCollisionHandler& handler,
    const Size index,
    const IBasicFinder& finder,
    FlatSet<Size>& toRemove,
    ArrayView<uint8_t> dirty) {
    Array<Tuple<Size, Size>> component;

    Array<NeighborRecord> neighs;
    for (Size i = 0; i < indices.size(); ++i) {
        if (indices[i] != index) {
            continue;
        }
        SPH_ASSERT(!dirty[i]);

        neighs.clear();
        finder.findAll(r[i], 2._f * r[i][H], neighs);

        Size neighCnt = 0;
        for (const NeighborRecord& n : neighs) {
            neighCnt += 1 - dirty[n.index];
        }

        component.push(makeTuple(i, neighCnt));
    }
    // start with particles that have the most neighbors
    std::sort(component.begin(),
        component.end(),
        [](const Tuple<Size, Size>& p1, const Tuple<Size, Size>& p2) { return p1.get<1>() > p2.get<1>(); });
    for (const Tuple<Size, Size>& p : component) {
        const Size i = p.get<0>();
        if (dirty[i]) {
            continue;
        }
        dirty[i] = 1;
        neighs.clear();
        finder.findAll(r[i], 2._f * r[i][H], neighs);

        for (const NeighborRecord& n : neighs) {
            const Size j = n.index;
            if (dirty[j]) {
                continue;
            }
            handler.collide(i, j, toRemove);
            dirty[j] = 1;
        }
    }
}

void mergeOverlappingSpheres(IScheduler& scheduler,
    Storage& storage,
    const Float surfacenessThreshold,
    const Size numIterations,
    const Size minComponentSize) {
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    storage.insert<Vector>(QuantityId::ANGULAR_FREQUENCY, OrderEnum::ZERO, Vector(0._f));

    // flag surface spheres
    KdTree<KdNode> finder;
    finder.build(scheduler, r, FinderFlag::SKIP_RANK);
    Array<uint8_t> surface = flagSurfaceParticles(scheduler, finder, r, surfacenessThreshold);


    for (Size iter = 0; iter < numIterations; ++iter) {
        // find connected components
        Array<Size> indices;
        const InnerParticleChecker checker(surface);
        const Size numComponents = Post::findComponents(storage, 2._f, checker, indices);
        Array<Size> componentSizes(numComponents);
        componentSizes.fill(0);
        for (Size i = 0; i < indices.size(); ++i) {
            componentSizes[indices[i]]++;
        }

        FlatSet<Size> toRemove;
        Array<uint8_t> dirty = surface.clone();
        MergingCollisionHandler handler(0, 0);
        handler.initialize(storage);

        for (Size index = 0; index < numComponents; ++index) {
            if (componentSizes[index] < minComponentSize) {
                // component too small, skip
                continue;
            }
            mergeComponent(r, indices, handler, index, finder, toRemove, dirty);
        }
        storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED | Storage::IndicesFlag::PROPAGATE);
        surface.remove(toRemove);

        r = storage.getValue<Vector>(QuantityId::POSITION);
        finder.build(scheduler, r, FinderFlag::SKIP_RANK);
    }
}

NAMESPACE_SPH_END
