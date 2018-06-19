#include "io/Output.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Box.h"
#include "openvdb/openvdb.h"
#include "quantities/IMaterial.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include <iostream>

using namespace Sph;

Storage loadSsf(const Path& path) {
    BinaryOutput io;
    Storage storage;
    Statistics stats;
    io.load(path, storage, stats);
    return storage;
}

INLINE openvdb::Vec3R vectorToVec3R(const Vector& v) {
    return openvdb::Vec3R(v[X], v[Y], v[Z]);
}

static constexpr Float SURFACE_LEVEL = 0.13_f;
static constexpr Float SCALE = 1.5e6_f;
static constexpr Size GRID_DIM = 200;

template <typename TFunctor>
void iterateBox(const Box& bbox, const TFunctor& functor) {
    const Float step = SCALE / GRID_DIM;

    auto iterateDim = [&bbox, step](const Coordinate dim, const auto& functor) {
        int i;
        Float x;
        for (x = 0, i = 0; x < bbox.upper()[dim]; x += step, ++i) {
            functor(x, i);
        }
        for (x = -step, i = -1; x > bbox.lower()[dim]; x -= step, --i) {
            functor(x, i);
        }
    };

    Size count = bbox.size()[X] / step;
    Size idx = 0;
    std::cout << std::endl;
    iterateDim(X, [&](Float x, int i) {
        std::cout << "\rProgress = " << (idx * 100 / count) << std::flush;
        idx++;
        iterateDim(Y, [&](Float y, int j) {
            iterateDim(Z, [&](Float z, int k) { //
                functor(Vector(x, y, z), Indices(i, j, k));
            });
        });
    });
}


void convert(const Storage& storage, openvdb::GridPtrVec& grids) {
    openvdb::FloatGrid::Ptr colorField = openvdb::FloatGrid::create(0._f);
    openvdb::Vec3SGrid::Ptr velocityField = openvdb::Vec3SGrid::create(vectorToVec3R(Vector(0._f)));
    openvdb::FloatGrid::Ptr energyField = openvdb::FloatGrid::create(0._f);

    typename openvdb::FloatGrid::Accessor colorAccessor = colorField->getAccessor();
    colorField->setName("Density");
    colorField->setGridClass(openvdb::GRID_LEVEL_SET);

    typename openvdb::Vec3SGrid::Accessor velocityAccessor = velocityField->getAccessor();
    velocityField->setName("Velocity");

    typename openvdb::FloatGrid::Accessor energyAccessor = energyField->getAccessor();
    energyField->setName("Emission");


    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);

    RunSettings settings;
    AutoPtr<IBasicFinder> finder = Factory::getFinder(settings);
    finder->build(r);

    LutKernel<3> kernel = Factory::getKernel<3>(settings);
    const Float u_iv = storage.getMaterial(0)->getParam<Float>(BodySettingsId::TILLOTSON_ENERGY_IV);

    Box bbox;
    Float radius = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        bbox.extend(r[i]);
        radius = max(radius, r[i][H]);
    }

    radius *= kernel.radius();
    bbox.extend(bbox.lower() - Vector(radius));
    bbox.extend(bbox.upper() + Vector(radius));

    bbox = Box(max(bbox.lower(), Vector(-4.e6_f)), min(bbox.upper(), Vector(4.e6_f)));

    // openvdb::Mat4R transform;
    // const Vector center = bbox.center() / maxElement(bbox.size());
    // const Vector scale = bbox.size() / maxElement(bbox.size());
    // transform.setToScale(openvdb::Vec3R(scale[X], scale[Y], scale[Z]));
    // transform.setTranslation(openvdb::Vec3R(center[X], center[Y], center[Z]));
    // auto tr = openvdb::math::Transform::createLinearTransform(transform);
    /*colorField->setTransform(tr);
    velocityField->setTransform(tr);
    energyField->setTransform(tr);*/


    Array<NeighbourRecord> neighs;

    iterateBox(bbox, [&](const Vector& pos, const Indices& idxs) {
        finder->findAll(pos, radius, neighs);
        Float field = 0._f;
        Float energy = 0._f;
        Vector velocity(0._f);
        Float wsum = 0._f;
        for (auto& n : neighs) {
            const Size j = n.index;
            const Float w = kernel.value(r[j] - pos, r[j][H]);
            const Float p = m[j] / 2700._f;
            field += p * w;
            energy += u[j] * p * w;
            velocity += v[j] * p * w;
            wsum += p * w;
        }
        if (wsum == 0._f) {
            return;
        }
        openvdb::Coord coords(idxs[0], idxs[1], idxs[2]);
        ASSERT(isReal(field));
        if (field > SURFACE_LEVEL) {
            colorAccessor.setValue(coords, field - SURFACE_LEVEL);
            velocityAccessor.setValue(coords, vectorToVec3R(velocity / wsum));

            const Float logEnergy = log(1._f + energy / wsum / u_iv);
            energyAccessor.setValue(coords, logEnergy);
        }
    });

    grids.push_back(colorField);
    // grids.push_back(velocityField);
    grids.push_back(energyField);
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: ssftovdb output.ssf grid.vdb" << std::endl;
        return 0;
    }
    Storage storage = loadSsf(Path(argv[1]));

    openvdb::initialize();

    openvdb::GridPtrVec grids;
    convert(storage, grids);

    openvdb::io::File file(argv[2]);
    file.write(grids);
    file.close();

    openvdb::uninitialize();
    return 0;
}
