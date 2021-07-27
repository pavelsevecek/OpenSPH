#include "objects/geometry/Delaunay.h"
#include "catch.hpp"
#include "io/Output.h"
#include "post/MeshFile.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Tetrahedron circumsphere", "[delaunay]") {
    Tetrahedron tet = Tetrahedron::unit();
    Vector center(1.5_f, -2.3_f, 4.1_f);
    Float radius = 2.4_f;
    for (Size i = 0; i < 4; ++i) {
        tet.vertex(i) = tet.vertex(i) * radius + center;
    }
    Sphere sphere = tet.circumsphere();
    REQUIRE(sphere.center() == approx(center));
    REQUIRE(sphere.radius() == approx(radius));

    center = Vector(0.4_f, -6.1_f, 3.14_f);
    radius = 3.6_f;
    UniformRng rng;
    for (Size i = 0; i < 4; ++i) {
        tet.vertex(i) = center + radius * sampleUnitSphere(rng);
    }
    sphere = tet.circumsphere();
    REQUIRE(sphere.center() == approx(center));
    REQUIRE(sphere.radius() == approx(radius));
}

TEST_CASE("Delaunay", "[delaunay]") {
    Delaunay delaunay;
    Array<Vector> points({ Vector(0, 0, 0), Vector(0, 0, 1), Vector(0, 1, 0), Vector(1, 0, 0) });
    delaunay.build(points);

    REQUIRE(delaunay.getTetrahedraCnt() == 1);

    /*Tetrahedron tet = delaunay.tetrahedron(0);
    for (Size i = 0; i < 4; ++i) {
        REQUIRE(Plane(tet.triangle(i)).signedDistance(tet.vertex(i)) < 0._f);
    }

    for (const Vector& p : points) {
        REQUIRE(delaunay.insideCell(0, p));
    }*/
}

TEST_CASE("Delaunay bunny", "[delaunay]") {
    BinaryInput input;
    Storage storage;
    Statistics stats;
    Outcome result = input.load(Path("/home/pavel/sandbox/bunny.ssf"), storage, stats);
    REQUIRE(result);

    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);
    std::random_shuffle(r.begin(), r.end());
    Delaunay delaunay;
    delaunay.build(r);
    PlyFile ply;
    Array<Triangle> triangles;
    for (Size i = 0; i < delaunay.getTetrahedraCnt(); ++i) {
        Tetrahedron tet = delaunay.tetrahedron(i);
        for (Size j = 0; j < 4; ++j) {
            triangles.push(tet.triangle(j));
        }
    }
    ply.save(Path("bunny.ply"), triangles);
}
