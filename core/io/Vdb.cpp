#include "io/Vdb.h"
#include "objects/geometry/Box.h"
#include "objects/geometry/Indices.h"
#include "quantities/IMaterial.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include <algorithm>

#ifdef SPH_USE_VDB
#include <openvdb/openvdb.h>
#endif

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_VDB

INLINE openvdb::Vec3f toVec3f(const Vector& v) {
    return openvdb::Vec3f(float(v[X]), float(v[Y]), float(v[Z]));
}

INLINE Vector toVector(const openvdb::Vec3f& v) {
    return Vector(v.x(), v.y(), v.z());
}

static Tuple<Indices, Indices> getParticleBox(const Vector& r, const openvdb::math::Transform& transform) {
    const Vector from = toVector(transform.worldToIndex(toVec3f(r - Vector(2._f * r[H]))));
    const Vector to = toVector(transform.worldToIndex(toVec3f(r + Vector(2._f * r[H]))));
    const Indices fromIdxs(Vector(ceil(from[X]), ceil(from[Y]), ceil(from[Z])));
    const Indices toIdxs(Vector(floor(to[X]), floor(to[Y]), floor(to[Z])));
    return { fromIdxs, toIdxs };
}

static float getVoxelSize(ArrayView<const Vector> r) {
    Array<float> h(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        h[i] = float(r[i][H]);
    }
    const Size mid = r.size() / 2;
    std::nth_element(h.begin(), h.begin() + mid, h.end());
    return h[mid];
}

VdbOutput::VdbOutput(const OutputFile& fileMask, const Float surfaceLevel)
    : IOutput(fileMask)
    , surfaceLevel(float(surfaceLevel)) {
    openvdb::initialize();
}

VdbOutput::~VdbOutput() {
    openvdb::uninitialize();
}

Expected<Path> VdbOutput::dump(const Storage& storage, const Statistics& stats) {
    using namespace openvdb;

    FloatGrid::Ptr colorField = FloatGrid::create(-surfaceLevel);
    Vec3SGrid::Ptr velocityField = Vec3SGrid::create(toVec3f(Vector(0._f)));
    FloatGrid::Ptr energyField = FloatGrid::create(0._f);

    colorField->setName("Density");
    velocityField->setName("Velocity");
    energyField->setName("Emission");

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);
    ArrayView<const Float> rho = storage.getValue<Float>(QuantityId::DENSITY);

    const float voxelSize = getVoxelSize(r);
    openvdb::math::Transform::Ptr transform = openvdb::math::Transform::createLinearTransform(voxelSize);

    LutKernel<3> kernel = Factory::getKernel<3>(RunSettings::getDefaults());

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
                        coord, [&u, c, i](float& energy) { energy += c * float(u[i]); });
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
        iter.setValue(c - surfaceLevel);
    }

    GridPtrVec vdbGrids;
    vdbGrids.push_back(colorField);
    vdbGrids.push_back(velocityField);
    vdbGrids.push_back(energyField);

    Path vdbPath = paths.getNextPath(stats);
    io::File vdbFile(vdbPath.string().toUtf8().cstr());
    vdbFile.write(vdbGrids);
    vdbFile.close();
    return vdbPath;
}

#endif

NAMESPACE_SPH_END
