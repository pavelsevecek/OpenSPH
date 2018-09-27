#include "io/FileSystem.h"
#include "io/Output.h"
#include "objects/containers/Grid.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Box.h"
#include "objects/geometry/Indices.h"
#include "openvdb/openvdb.h"
#include "quantities/IMaterial.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "thread/Pool.h"
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

struct VdbParams {

    Box box = Box(Vector(-5.e5_f, -5.e5_f, -3.e5_f), Vector(5.e5_f, 5.e5_f, 3.e5_f));

    Indices gridDims = Indices(1024);

    Float surfaceLevel = 0.13_f;
};

Vector worldToRelative(const Vector& r, const VdbParams& params) {
    return (r - params.box.lower()) / params.box.size() * Vector(params.gridDims);
}

Vector relativeToWorld(const Vector& r, const VdbParams& params) {
    return r * params.box.size() / Vector(params.gridDims) + params.box.lower();
}

Tuple<Indices, Indices> getParticleBox(const Vector& r, const VdbParams& params) {
    const Vector from = worldToRelative(r - Vector(2._f * r[H]), params);
    const Vector to = worldToRelative(r + Vector(2._f * r[H]), params);
    const Indices fromIdxs(ceil(from[X]), ceil(from[Y]), ceil(from[Z]));
    const Indices toIdxs(floor(to[X]), floor(to[Y]), floor(to[Z]));
    return { max(fromIdxs, Indices(0._f)), min(toIdxs, params.gridDims - Indices(1)) };
}

void convert(const Path& path, const VdbParams params) {
    const Storage storage = loadSsf(path);
    openvdb::GridPtrVec vdbGrids;


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
    // ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<const Float> u = storage.getValue<Float>(QuantityId::ENERGY);

    const Float u_iv = storage.getMaterial(0)->getParam<Float>(BodySettingsId::TILLOTSON_ENERGY_IV);


    const Size gridSize = max(params.gridDims[0], params.gridDims[1], params.gridDims[2]);

    struct Record {
        float density = 0.f;
        float energy = 0.f;
    };

    SparseGrid<Record> grid(gridSize);
    // Grid<Vector> velocity(params.gridDims, Vector(0._f));

    LutKernel<3> kernel = Factory::getKernel<3>(RunSettings::getDefaults());

    // Timer timer;

    for (Size i = 0; i < r.size(); ++i) {
        Indices from, to;
        tieToTuple(from, to) = getParticleBox(r[i], params);
        for (int x = from[X]; x <= to[X]; ++x) {
            for (int y = from[Y]; y <= to[Y]; ++y) {
                for (int z = from[Z]; z <= to[Z]; ++z) {
                    const Indices idxs(x, y, z);
                    const Vector pos = relativeToWorld(idxs, params);
                    const Float w = kernel.value(r[i] - pos, r[i][H]);
                    const Float p = m[i] / 2700._f;
                    Record& record = grid[idxs];
                    record.density += p * w;
                    record.energy += u[i] * p * w;
                    // velocity[idxs] += v[j] * p * w;
                }
            }
        }
    }
    // std::cout << "Grid created in " << timer.elapsed(TimerUnit::MILLISECOND) << "ms" << std::endl;

    // timer.restart();
    /*    for (int x = 0; x < params.gridDims[X]; ++x) {
            for (int y = 0; y < params.gridDims[Y]; ++y) {
                for (int z = 0; z < params.gridDims[Z]; ++z) {*/
    grid.iterate([&](const Record& record, const Indices idxs) {
        ASSERT(isReal(record.density));
        ASSERT(isReal(record.energy));
        openvdb::Coord coords(idxs[0], idxs[1], idxs[2]);

        if (record.density < params.surfaceLevel) {
            return;
        }
        colorAccessor.setValue(coords, record.density - params.surfaceLevel);

        // const Vector v = velocity[idxs] / field;
        // velocityAccessor.setValue(coords, vectorToVec3R(v));

        const Float e = log(1._f + record.energy / record.density / u_iv);
        energyAccessor.setValue(coords, e);
    });

    // zstd::cout << "Converted to VDB grid in " << timer.elapsed(TimerUnit::MILLISECOND) << "ms" <<
    // std::endl;

    /*            }
            }
        }*/


    vdbGrids.push_back(colorField);
    // grids.push_back(velocityField);
    vdbGrids.push_back(energyField);

    Path vdbPath = path;
    vdbPath.replaceExtension("vdb");
    openvdb::io::File vdbFile(vdbPath.native());
    vdbFile.write(vdbGrids);
    vdbFile.close();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ssftovdb parentDir" << std::endl;
        return 0;
    }
    openvdb::initialize();

    VdbParams params;

    const Path directory(argv[1]);
    std::set<Path> paths;
    for (Path file : FileSystem::iterateDirectory(directory)) {
        if (file.extension() != Path("ssf")) {
            continue;
        }
        paths.insert(file);
    }

    std::mutex mutex;
    ThreadPool pool;
    for (Path file : paths) {
        pool.submit([&params, &mutex, file] {
            {
                std::unique_lock<std::mutex> lock(mutex);
                std::cout << "Processing: " << file.native() << std::endl;
            }

            convert(file, params);
        });
    }

    pool.waitForAll();

    openvdb::uninitialize();
    return 0;
}
