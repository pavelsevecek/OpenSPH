#include "objects/geometry/Delaunay.h"
#include "catch.hpp"
#include "io/Output.h"
#include "objects/finders/NeighborFinder.h"
#include "post/MeshFile.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "tests/Approx.h"
#include "thread/Scheduler.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Tetrahedron basic", "[delaunay]") {
    Tetrahedron tet = Tetrahedron::unit();
    const Float a = sqrt(8._f / 3._f);
    REQUIRE(tet.volume() == approx(pow<3>(a) / (6._f * sqrt(2._f))));
    REQUIRE(tet.contains(Vector(0._f)));
    REQUIRE(tet.center() == approx(Vector(0._f)));
    REQUIRE(tet.contains(tet.center()));
}

TEST_CASE("Tetrahedron circumsphere", "[delaunay]") {
    Tetrahedron tet = Tetrahedron::unit();
    Vector center(1.5_f, -2.3_f, 4.1_f);
    Float radius = 2.4_f;
    for (Size i = 0; i < 4; ++i) {
        tet.vertex(i) = tet.vertex(i) * radius + center;
    }
    Sphere sphere = tet.circumsphere().value();
    REQUIRE(sphere.center() == approx(center));
    REQUIRE(sphere.radius() == approx(radius));

    center = Vector(0.4_f, -6.1_f, 3.14_f);
    radius = 3.6_f;
    UniformRng rng;
    for (Size i = 0; i < 4; ++i) {
        tet.vertex(i) = center + radius * sampleUnitSphere(rng);
    }
    sphere = tet.circumsphere().value();
    REQUIRE(sphere.center() == approx(center));
    REQUIRE(sphere.radius() == approx(radius));
}

TEST_CASE("Tetrahedron circumsphere coplanar", "[delaunay]") {
    Tetrahedron tet;
    tet.vertex(0) = Vector(0, 0, 0);
    tet.vertex(1) = Vector(1, 0, 0);
    tet.vertex(2) = Vector(0, 1, 0);
    tet.vertex(3) = Vector(1, 1, 0);
    REQUIRE_FALSE(tet.circumsphere());
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
    Array<Size> toRemove;
    if (false) {
        AutoPtr<IBasicFinder> finder = Factory::getFinder(RunSettings::getDefaults());
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        finder->build(SEQUENTIAL, r);
        Array<NeighborRecord> neighs;
        for (Size i = 0; i < r.size(); ++i) {
            finder->findAll(r[i], 2._f * r[i][H], neighs);
            if (neighs.size() > 50) {
                toRemove.push(i);
            }
        }
    }
    storage.remove(toRemove, Storage::IndicesFlag::INDICES_SORTED);

    Array<Vector>& r = storage.getValue<Vector>(QuantityId::POSITION);
    std::random_shuffle(r.begin(), r.end());
    Delaunay delaunay;
    delaunay.build(r);
    PlyFile ply;
    /*Array<Triangle> triangles;
    for (Size i = 0; i < delaunay.getTetrahedraCnt(); ++i) {
        Tetrahedron tet = delaunay.tetrahedron(i);
        for (Size j = 0; j < 4; ++j) {
            triangles.push(tet.triangle(j));
        }
    }
    ply.save(Path("bunny.ply"), triangles);*/

    ply.save(Path("bunny-ch.ply"), delaunay.convexHull());
    ply.save(Path("bunny-alpha.ply"), delaunay.alphaShape(0.012));
}
