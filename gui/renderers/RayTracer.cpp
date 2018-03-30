#include "gui/renderers/RayTracer.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "objects/finders/KdTree.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

RayTracer::RayTracer(const GuiSettings& settings)
    : threadData(pool) {
    kernel = Factory::getKernel<3>(RunSettings::getDefaults());
    params.surfaceLevel = settings.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    params.brdf = Factory::getBrdf(settings);
}

RayTracer::~RayTracer() = default;

void RayTracer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    MEASURE_SCOPE("Building BVH");
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    const Size particleCnt = cached.r.size();

    cached.flags.resize(particleCnt);
    ArrayView<const Size> idxs = storage.getValue<Size>(QuantityId::FLAG);
    ArrayView<const Float> reduce = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
    // assign separate flag to fully damaged particles so that they do not blend with other particles
    Size damagedIdx = 5;
    for (Size i = 0; i < particleCnt; ++i) {
        if (reduce[i] == 0._f) {
            cached.flags[i] = damagedIdx++;
        } else {
            cached.flags[i] = idxs[i];
        }
    }

    ArrayView<const Float> rho, m;
    tie(rho, m) = storage.getValues<Float>(QuantityId::DENSITY, QuantityId::MASS);
    cached.v.resize(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        cached.v[i] = m[i] / rho[i];
    }

    cached.colors.resize(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        cached.colors[i] = colorizer.evalColor(i);
    }

    Array<BvhSphere> spheres;
    spheres.reserve(particleCnt);
    for (Size i = 0; i < particleCnt; ++i) {
        /// \todo hardcoded for r_kernel = 2
        BvhSphere& s = spheres.emplaceBack(cached.r[i], 1.5_f * cached.r[i][H]);
        s.userData = i;
    }
    bvh.build(std::move(spheres));

    finder = makeAuto<KdTree>();
    finder->build(cached.r);

    threadData.forEach([](ThreadData& data) { data.previousIdx = Size(-1); });
}

SharedPtr<wxBitmap> RayTracer::render(const ICamera& camera,
    const RenderParams& params,
    Statistics& UNUSED(stats)) const {
    MEASURE_SCOPE("Rendering frame");
    Bitmap bitmap(params.size);
    /// \todo this currently works only for ortho camera
    const Float ratio =
        getLength(camera.unproject(Point(1, 0)).origin - camera.unproject(Point(1, 0)).origin);
    parallelFor(pool, 0, Size(params.size.y), [this, &bitmap, &camera, &params, ratio](Size y) {
        ThreadData& data = threadData.get();
        for (Size x = 0; x < Size(params.size.x); ++x) {
            CameraRay cameraRay = camera.unproject(Point(x, y));
            const Ray ray(cameraRay.origin, getNormalized(cameraRay.target - cameraRay.origin));
            bvh.getAllIntersections(ray, data.intersections);
            Color accumulatedColor = Color::black();
            for (IntersectionInfo& intersect : data.intersections) {
                ShadeContext sc;
                sc.index = intersect.object->userData;
                sc.ray = ray;
                sc.t_min = intersect.t;
                sc.pixelToWorldRatio = ratio;
                const Optional<Color> color = this->shade(data, sc);
                if (color) {
                    accumulatedColor = color.value();
                    break;
                }
                // rejected, process another intersection
            }
            bitmap[Point(x, y)] = wxColour(accumulatedColor);
        }
    });
    SharedPtr<wxBitmap> wx = makeShared<wxBitmap>();
    toWxBitmap(bitmap, *wx);
    return wx;
}

Optional<Color> RayTracer::shade(ThreadData& data, const ShadeContext& context) const {
    // look for neighbours only if the intersected particle differs from the previous one
    const Size i = context.index;
    if (i != data.previousIdx) {
        finder->findAll(i, 3._f * cached.r[i][H], data.neighs);
        data.previousIdx = i;
    }
    // find the actual list of neighbours
    Array<Size> actNeighs;
    for (NeighbourRecord& n : data.neighs) {
        if (cached.flags[i] == cached.flags[n.index]) {
            actNeighs.push(n.index);
        }
    }

    const Ray& ray = context.ray;
    ASSERT(getSqrLength(ray.direction()) == 1._f, getSqrLength(ray.direction()));
    Vector v1 = ray.origin() + ray.direction() * context.t_min;
    // the sphere hit should be always above the surface
    ASSERT(this->evalField(actNeighs, v1) < 0._f);
    // look for the intersection up to hit + 4H; if we don't find it, we should reject the hit and look for
    // the next intersection - the surface can be non-convex!!
    const Float limit = 3._f * cached.r[i][H];
    // initial step - cannot be too large otherwise the ray could 'tunnel through' on grazing angles
    Float eps = 0.1_f * cached.r[i][H];
    Vector v2 = v1 + eps * ray.direction();

    Float phi = 0._f;
    Float travelled = eps;
    while (travelled < limit && eps > 0.01_f * cached.r[i][H]) {
        phi = this->evalField(actNeighs, v2);
        if (phi > 0._f) {
            // we crossed the surface, move back
            v2 = 0.5_f * (v1 + v2);
            eps *= 0.5_f;
            // since we crossed the surface, don't check for travelled distance anymore
            travelled = -INFTY;
        } else {
            // we are still above the surface, move further
            v1 = v2;
            v2 += eps * ray.direction();
            travelled += eps;
        }
    }

    if (travelled >= limit) {
        // didn't find surface, reject the hit
        return NOTHING;
    } else {
        // compute gradient
        const Float ratio = context.pixelToWorldRatio;
        const Float phi_dx = this->evalField(actNeighs, v2 + Vector(ratio, 0._f, 0._f));
        const Float phi_dy = this->evalField(actNeighs, v2 + Vector(0._f, ratio, 0._f));
        const Float phi_dz = this->evalField(actNeighs, v2 + Vector(0._f, 0._f, ratio));
        const Vector n(phi_dx - phi, phi_dy - phi, phi_dz - phi);
        ASSERT(n != Vector(0._f));
        const Vector n_norm = getNormalized(n);

        return this->evalColor(actNeighs, v2) * abs(n_norm[Z]);
    }
}

Float RayTracer::evalField(ArrayView<const Size> neighs, const Vector& pos1) const {
    ASSERT(!neighs.empty());
    Float value = 0._f;
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        /// \todo could be optimized by using n.distSqr, no need to compute the dot again
        const Float w = kernel.value(pos1 - pos2, pos2[H]);
        value += cached.v[index] * w;
    }
    return value - params.surfaceLevel;
}

Color RayTracer::evalColor(ArrayView<const Size> neighs, const Vector& pos1) const {
    ASSERT(!neighs.empty());
    Color color = Color::black();
    Float weightSum = 0._f;
    for (Size index : neighs) {
        const Vector& pos2 = cached.r[index];
        /// \todo could be optimized by using n.distSqr, no need to compute the dot again
        const Float w = kernel.value(pos1 - pos2, pos2[H]) * cached.v[index];
        color += cached.colors[index] * w;
        weightSum += w;
    }
    ASSERT(weightSum != 0._f);
    return color / weightSum;
}

NAMESPACE_SPH_END
