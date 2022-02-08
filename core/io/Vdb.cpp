#include "io/Vdb.h"
#include "objects/finders/KdTree.h"
#include "objects/geometry/Box.h"
#include "objects/geometry/Indices.h"
#include "quantities/IMaterial.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "thread/ThreadLocal.h"
#include <algorithm>

#ifdef SPH_USE_VDB
#include <openvdb/openvdb.h>

using namespace openvdb;
#endif

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_VDB

INLINE Vec3f toVec3f(const Vector& v) {
    return openvdb::Vec3f(float(v[X]), float(v[Y]), float(v[Z]));
}

INLINE Vector toVector(const Vec3f& v) {
    return Vector(v.x(), v.y(), v.z());
}

static Tuple<Indices, Indices> getParticleBox(const Vector& r, const math::Transform& transform) {
    const Vector from = toVector(transform.worldToIndex(toVec3f(r - Vector(2._f * r[H]))));
    const Vector to = toVector(transform.worldToIndex(toVec3f(r + Vector(2._f * r[H]))));
    const Indices fromIdxs(Vector(ceil(from[X]), ceil(from[Y]), ceil(from[Z])));
    const Indices toIdxs(Vector(floor(to[X]), floor(to[Y]), floor(to[Z])));
    return { fromIdxs, toIdxs };
}

template <typename T>
static T median(Array<T>&& values) {
    const Size mid = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + mid, values.end());
    return values[mid];
}

template <typename T>
static T median(ArrayView<const T> values) {
    Array<T> copy;
    copy.pushAll(values.begin(), values.end());
    return median(std::move(copy));
}

static float getVoxelSize(ArrayView<const Vector> r) {
    Array<float> h(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        h[i] = float(r[i][H]);
    }
    return median(std::move(h));
}

VdbOutput::VdbOutput(const OutputFile& fileMask, const Float surfaceLevel)
    : IOutput(fileMask)
    , surfaceLevel(float(surfaceLevel)) {
    openvdb::initialize();
}

VdbOutput::~VdbOutput() {
    openvdb::uninitialize();
}

constexpr Size MIN_NEIGHT = 8;
constexpr Float MAX_DISTENTION = 50;

static Tuple<Array<Float>, Float> getDensities(ArrayView<const Float> m,
    ArrayView<const Vector> r,
    const LutKernel<3>& kernel) {
    KdTree<KdNode> finder;
    SharedPtr<IScheduler> scheduler = Factory::getScheduler();
    finder.build(*scheduler, r, FinderFlag::SKIP_RANK);
    ThreadLocal<Array<NeighborRecord>> neighsTl(*scheduler);
    Array<Float> rho(r.size());
    Array<Float> distentions(r.size());
    parallelFor(*scheduler, neighsTl, 0, r.size(), [&](const Size i, Array<NeighborRecord>& neighs) {
        Float radius = 2._f;
        for (; radius < MAX_DISTENTION; radius *= 2) {
            finder.findAll(r[i], r[i][H] * radius, neighs);
            if (neighs.size() >= MIN_NEIGHT) {
                break;
            }
        }
        rho[i] = 0._f;
        for (const NeighborRecord& n : neighs) {
            const Size j = n.index;
            const Float w = kernel.value(r[i] - r[j], r[i][H] * radius);
            rho[i] += m[j] * w;
        }
        distentions[i] = radius;
    });
    return makeTuple(std::move(rho), median(std::move(distentions)));
}

static GridPtrVec particlesToGrids(const Storage& storage) {
    FloatGrid::Ptr colorField = FloatGrid::create(0._f);
    Vec3SGrid::Ptr velocityField = Vec3SGrid::create(toVec3f(Vector(0._f)));
    FloatGrid::Ptr energyField = FloatGrid::create(0._f);

    colorField->setName("density");
    velocityField->setName("velocity");
    energyField->setName("emission");

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);

    LutKernel<3> kernel = Factory::getKernel<3>(RunSettings::getDefaults());
    Array<Float> rho;
    Float distention;
    if (storage.has(QuantityId::DENSITY)) {
        rho = storage.getValue<Float>(QuantityId::DENSITY).clone();
        distention = 1;
    } else {
        tieToTuple(rho, distention) = getDensities(m, r, kernel);
    }
    Array<Float> e;
    if (storage.has(QuantityId::ENERGY)) {
        e = storage.getValue<Float>(QuantityId::ENERGY).clone();
    } else {
        ArrayView<const Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
        e.resize(dv.size());
        for (Size i = 0; i < dv.size(); ++i) {
            e[i] = getLength(dv[i]);
        }
    }
    const Float e0 = median<Float>(e.view());

    const float voxelSize = float(getVoxelSize(r) * distention);
    openvdb::math::Transform::Ptr transform = openvdb::math::Transform::createLinearTransform(voxelSize);

    typename FloatGrid::Accessor colorAccessor = colorField->getAccessor();
    typename Vec3SGrid::Accessor velocityAccessor = velocityField->getAccessor();
    typename FloatGrid::Accessor energyAccessor = energyField->getAccessor();
    for (Size i = 0; i < r.size(); ++i) {
        Indices from, to;
        tieToTuple(from, to) = getParticleBox(r[i], *transform);
        Float rho_i;
        if (storage.getMaterialCnt() > 0) {
            rho_i = storage.getMaterialOfParticle(i)->getParam<Float>(BodySettingsId::DENSITY);
        } else {
            rho_i = rho[i];
        }
        for (int x = from[X]; x <= to[X]; ++x) {
            for (int y = from[Y]; y <= to[Y]; ++y) {
                for (int z = from[Z]; z <= to[Z]; ++z) {
                    const Vec3f idxs(x, y, z);
                    const Vector pos = toVector(transform->indexToWorld(idxs));
                    const float w = float(kernel.value(r[i] - pos, r[i][H]));
                    const float c = float(m[i] / rho_i * w);

                    const Coord coord(x, y, z);
                    colorAccessor.modifyValue(coord, [c](float& color) { color += c; });
                    energyAccessor.modifyValue(
                        coord, [&e, e0, c, i](float& energy) { energy += c * float(e[i] / e0); });
                    velocityAccessor.modifyValue(
                        coord, [&v, c, i](Vec3f& velocity) { velocity += c * toVec3f(v[i]); });
                }
            }
        }
    }

    for (FloatGrid::ValueOnIter iter = colorField->beginValueOn(); iter; ++iter) {
        const Coord coord = iter.getCoord();
        const float c = *iter;
        if (c > 0) {
            energyAccessor.modifyValue(coord, [c](float& energy) { energy /= c; });
            velocityAccessor.modifyValue(coord, [c](Vec3f& velocity) { velocity /= c; });
        }
        iter.setValue(c);
    }

    GridPtrVec grids;
    grids.push_back(colorField);
    grids.push_back(velocityField);
    grids.push_back(energyField);
    return grids;
}

Expected<Path> VdbOutput::dump(const Storage& storage, const Statistics& stats) {
    GridPtrVec vdbGrids;
    try {
        vdbGrids = particlesToGrids(storage);
    } catch (const std::exception& e) {
        return makeUnexpected<Path>("Failed to create VDB grid.\n{}", e.what());
    }
    Path vdbPath = paths.getNextPath(stats);
    io::File vdbFile(vdbPath.string().toUtf8().cstr());
    vdbFile.write(vdbGrids);
    vdbFile.close();
    return vdbPath;
}

#endif

NAMESPACE_SPH_END
