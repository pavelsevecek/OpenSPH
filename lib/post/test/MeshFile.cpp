#include "post/MeshFile.h"
#include "catch.hpp"
#include "io/FileSystem.h"

using namespace Sph;

TEST_CASE("MeshFile Triangle conversion", "[meshfile]") {
    struct DummyPlyFile : public PlyFile { // hack to access protected member function
    public:
        void convert(ArrayView<const Triangle> triangles, Array<Vector>& vertices, Array<Size>& indices) {
            this->getVerticesAndIndices(triangles, vertices, indices, 0.f);
        }
    };

    // simple tetrahedron
    Array<Vector> vertices{ Vector(-1, 0, 0), Vector(1, 0, 0), Vector(0, 1, 0), Vector(0, 0.3, 1) };
    Array<Triangle> triangles{ Triangle(vertices[0], vertices[1], vertices[2]),
        Triangle(vertices[0], vertices[1], vertices[3]),
        Triangle(vertices[0], vertices[2], vertices[3]),
        Triangle(vertices[1], vertices[2], vertices[3]) };

    Array<Vector> outVtxs;
    Array<Size> outIdxs;
    DummyPlyFile().convert(triangles, outVtxs, outIdxs);

    REQUIRE(outVtxs == vertices);
    REQUIRE(outIdxs == Array<Size>({ 0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3 }));
}

TEST_CASE("Ply cube", "[meshfile]") {
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
