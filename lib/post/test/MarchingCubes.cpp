#include "post/MarchingCubes.h"
#include "catch.hpp"
#include "post/MeshFile.h"
#include "quantities/Storage.h"

using namespace Sph;

TEST_CASE("MarchingCubes sphere", "[marchingcubes]") {
    struct SphereField : public Abstract::ScalarField {
        virtual Float operator()(const Vector& r) override {
            return getLength(r);
        }
    };

    Array<Vector> r;
    Box box(Vector(-1._f), Vector(1._f));
    box.iterate(Vector(1._f), [&r](const Vector& pos) { r.push(pos); });

    MarchingCubes mc(r, 0.4_f, makeAuto<SphereField>());
    mc.addComponent(box, 0.049_f);
    Array<Triangle>& triangles = mc.getTriangles();

    PlyFile file;
    REQUIRE(file.save(Path("mc.ply"), triangles));
}
