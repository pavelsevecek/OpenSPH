#include "post/MeshFile.h"
#include "catch.hpp"
#include "io/FileSystem.h"

/*#include "io/Logger.h"
#include "post/MeshDomain.h"
#include "sph/initial/Distribution.h"
#include "thread/Scheduler.h"*/

using namespace Sph;

TEST_CASE("MeshFile getVerticesAndIndices", "[meshfile]") {
    // simple tetrahedron
    Array<Vector> vertices{ Vector(-1, 0, 0), Vector(1, 0, 0), Vector(0, 1, 0), Vector(0, 0.3, 1) };
    Array<Triangle> triangles{ Triangle(vertices[0], vertices[1], vertices[2]),
        Triangle(vertices[0], vertices[1], vertices[3]),
        Triangle(vertices[0], vertices[2], vertices[3]),
        Triangle(vertices[1], vertices[2], vertices[3]) };

    Array<Vector> outVtxs;
    Array<Size> outIdxs;
    getVerticesAndIndices(triangles, outVtxs, outIdxs, 0.f);

    REQUIRE(outVtxs == vertices);
    REQUIRE(outIdxs == Array<Size>({ 0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3 }));
}

TEST_CASE("Ply save cube", "[meshfile]") {
    PlyFile file;

    Array<Vector> vertices{
        Vector(0, 0, 0),
        Vector(1, 0, 0),
        Vector(1, 1, 0),
        Vector(0, 1, 0),
        Vector(0, 0, 1),
        Vector(1, 0, 1),
        Vector(1, 1, 1),
        Vector(0, 1, 1),
    };
    Array<Triangle> triangles{
        // bottom
        Triangle(vertices[0], vertices[2], vertices[1]),
        Triangle(vertices[0], vertices[3], vertices[2]),
        // top
        Triangle(vertices[4], vertices[5], vertices[6]),
        Triangle(vertices[4], vertices[6], vertices[7]),
        // left
        Triangle(vertices[0], vertices[4], vertices[7]),
        Triangle(vertices[0], vertices[7], vertices[3]),
        // right
        Triangle(vertices[1], vertices[2], vertices[6]),
        Triangle(vertices[1], vertices[6], vertices[5]),
        // front
        Triangle(vertices[0], vertices[1], vertices[5]),
        Triangle(vertices[0], vertices[5], vertices[4]),
        // back
        Triangle(vertices[2], vertices[3], vertices[7]),
        Triangle(vertices[2], vertices[7], vertices[6]),
    };

    REQUIRE(file.save(Path("cube.ply"), triangles));

    std::string plyData = FileSystem::readFile(Path("cube.ply"));
    std::size_t n = plyData.find("element vertex");
    REQUIRE(n != std::string::npos);

    Size vertexCnt = std::stoi(plyData.substr(n + 15, 2));
    REQUIRE(vertexCnt == 8);

    n = plyData.find("element face");
    REQUIRE(n != std::string::npos);

    Size faceCnt = std::stoi(plyData.substr(n + 13, 3));
    REQUIRE(faceCnt == 12);
}

TEST_CASE("Ply load cube", "[meshfile]") {
    PlyFile file;
    Expected<Array<Triangle>> triangles = file.load(Path("cube.ply"));
    REQUIRE(triangles);
    REQUIRE(triangles->size() == 12); // six squares -> 12 triangles
}

/*TEST_CASE("Test bunny", "[meshfile]") {
    PlyFile file;
    Path bunnyPath("/home/pavel/projects/astro/sph/external/bunny/reconstruction/bun_zipper.ply");
    Expected<Array<Triangle>> triangles = file.load(bunnyPath);
    REQUIRE(triangles);

    MeshDomain domain(std::move(triangles.value()), AffineMatrix::rotateX(PI / 2._f));
    HexagonalPacking packing;
    Array<Vector> r = packing.generate(SEQUENTIAL, 300000, domain);
    FileLogger logger(Path("bunny.txt"));
    for (Size i = 0; i < r.size(); ++i) {
        logger.write(r[i][X], "  ", r[i][Y], "  ", r[i][Z]);
    }
}*/
