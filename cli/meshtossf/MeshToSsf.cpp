#include "Sph.h"
#include "post/MeshFile.h"
#include "sph/initial/MeshDomain.h"
#include <iostream>

using namespace Sph;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: meshtossf mesh.ext out.ssf" << std::endl;
        return 0;
    }

    Path meshPath(argv[1]);
    AutoPtr<IMeshFile> meshLoader;
    if (meshPath.extension() == Path("ply")) {
        meshLoader = makeAuto<PlyFile>();
    } else if (meshPath.extension() == Path("tab")) {
        meshLoader = makeAuto<TabFile>();
    } else {
        std::cout << "Unknown file format: " << meshPath.extension().native() << std::endl;
        return -1;
    }

    Expected<Array<Triangle>> triangles = meshLoader->load(meshPath);
    if (!triangles) {
        std::cout << "Cannot load mesh file: " << triangles.error() << std::endl;
        return -1;
    }

    MeshDomain domain(std::move(triangles.value())); // , AffineMatrix::rotateX(PI / 2._f));
    HexagonalPacking packing;
    Array<Vector> r = packing.generate(SEQUENTIAL, 200000, domain);
    Storage storage(makeAuto<NullMaterial>(BodySettings::getDefaults()));
    storage.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(r));
    Statistics stats;
    stats.set(StatisticsId::RUN_TIME, 0._f);
    Path ssfPath(argv[2]);
    BinaryOutput output(ssfPath);

    output.dump(storage, stats);

    return 0;
}
