#include "post/MarchingCubes.h"
#include "catch.hpp"
#include "objects/geometry/Domain.h"
#include "post/MeshFile.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"
#include "tests/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("MarchingCubes sphere", "[marchingcubes]") {
    struct SphereField : public IScalarField {
        virtual Float operator()(const Vector& r) override {
            return getLength(r);
        }
    };

    Array<Vector> r;
    Box box(Vector(-1._f), Vector(1._f));
    box.iterate(Vector(1._f), [&r](const Vector& pos) { r.push(pos); });

    const Float r0 = 0.4_f;
    MarchingCubes mc(r, r0, makeAuto<SphereField>());
    mc.addComponent(box, 0.05_f);
    Array<Triangle>& triangles = mc.getTriangles();

    auto test = [&](const Size i) -> Outcome {
        Triangle& t = triangles[i];
        for (Size i = 0; i < 3; ++i) {
            if (getLength(t[i]) != approx(0.4_f, 1.e-3_f)) {
                return makeFailed("Invalid vertex position: ", t[i], ", r = ", getLength(t[i]));
            }
        }
        return SUCCESS;
    };

    REQUIRE_SEQUENCE(test, 0, triangles.size());

    /*PlyFile file;
    REQUIRE(file.save(Path("mc.ply"), triangles));*/
}

TEST_CASE("MarchingCubes storage", "[marchingcubes]") {
    Storage storage;
    InitialConditions initial(RunSettings::getDefaults());
    BodySettings body;
    body.set(BodySettingsId::PARTICLE_COUNT, 10000);
    initial.addMonolithicBody(storage, SphericalDomain(Vector(0._f), 1._f), body);
    body.set(BodySettingsId::PARTICLE_COUNT, 100);
    initial.addMonolithicBody(storage, SphericalDomain(spherical(1._f, PI / 4._f, PI / 4._f), 0.2_f), body);
    initial.addMonolithicBody(storage, SphericalDomain(spherical(1._f, PI / 4._f, -PI / 4._f), 0.2_f), body);


    Array<Triangle> triangles = getSurfaceMesh(storage, 0.05_f, 0.2_f);

    PlyFile file;
    REQUIRE(file.save(Path("storage.ply"), triangles));
}
