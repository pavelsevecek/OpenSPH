#include "objects/geometry/Delaunay.h"
#include "catch.hpp"
#include "objects/finders/NeighborFinder.h"
#include "objects/geometry/Domain.h"
#include "objects/utility/Algorithm.h"
#include "sph/initial/Distribution.h"
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

TEST_CASE("Tetrahedron contains", "[delaunay]") {
    Tetrahedron tet(Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 1, 0), Vector(0, 0, 1));
    REQUIRE(tet.contains(Vector(0.001_f)));
    REQUIRE(tet.contains(Vector(0.25_f)));
    REQUIRE(tet.contains(Vector(0.32_f)));
    REQUIRE_FALSE(tet.contains(Vector(1._f, 0.25_f, 0.25_f)));
    REQUIRE_FALSE(tet.contains(Vector(0.25_f, 1._f, 0.25_f)));
    REQUIRE_FALSE(tet.contains(Vector(-0.01_f, 0.01_f, 0.01_f)));

    Tetrahedron inv(Vector(0, 0, 0), Vector(0, 1, 0), Vector(1, 0, 0), Vector(0, 0, 1));
    REQUIRE_SPH_ASSERT(inv.contains(Vector(0.25_f)));
}

TEST_CASE("Tetrahedron volume", "[delaunay]") {
    Tetrahedron tet(Vector(0, 0, 0), Vector(1, 0, 0), Vector(0, 1, 0), Vector(0, 0, 1));
    REQUIRE(tet.volume() == approx(1._f / 6._f));
    REQUIRE(tet.signedVolume() == approx(1._f / 6._f));

    Tetrahedron inv(Vector(0, 0, 0), Vector(0, 1, 0), Vector(1, 0, 0), Vector(0, 0, 1));
    REQUIRE(inv.volume() == approx(1._f / 6._f));
    REQUIRE(inv.signedVolume() == approx(-1._f / 6._f));
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

TEST_CASE("Delaunay single tetrahedron", "[delaunay]") {
    Tetrahedron expected(Vector(0, 0, 0), Vector(0, 0, 1), Vector(0, 1, 0), Vector(1, 0, 0));

    Delaunay delaunay;
    Array<Vector> points({ expected.vertex(0), expected.vertex(1), expected.vertex(2), expected.vertex(3) });
    delaunay.build(points);

    REQUIRE(delaunay.getCellCnt() == 1);
    Delaunay::Cell::Handle cell = delaunay.getCell(0);
    REQUIRE(cell->getNeighborCnt() == 0);
    REQUIRE(cell->neighbor(0) == nullptr);
    REQUIRE_SPH_ASSERT(cell->mirror(0));

    Tetrahedron actual = delaunay.tetrahedron(*cell);
    REQUIRE(actual.vertex(0) == expected.vertex(0));
    REQUIRE(actual.vertex(2) == expected.vertex(1));
    REQUIRE(actual.vertex(1) == expected.vertex(2));
    REQUIRE(actual.vertex(3) == expected.vertex(3));
    REQUIRE(actual.signedVolume() == actual.volume());
}

TEST_CASE("Delaunay sphere", "[delaunay]") {
    const Float radius = 500._f;
    SphericalDomain domain(Vector(0._f), radius);
    RandomDistribution distr(1234);
    Array<Vector> r = distr.generate(SEQUENTIAL, 10000, domain);

    Delaunay delaunay;
    SECTION("Delaunay spatial sort") {
        delaunay.build(r, Delaunay::BuildFlag::SPATIAL_SORT);
    }
    SECTION("Delaunay unsorted") {
        delaunay.build(r, EMPTY_FLAGS);
    }
    REQUIRE(delaunay.getCellCnt() == 65302);

    Array<Triangle> tris = delaunay.convexHull();
    const bool hullCorrect = allMatching(tris, [radius](const Triangle& t) {
        for (Size i = 0; i < 3; ++i) {
            const Float diff = abs(getLength(t[i]) - radius);
            if (diff > 20._f) {
                return false;
            }
        }
        return true;
    });
    REQUIRE(hullCorrect);
}

TEST_CASE("Delaunay locate point", "[delaunay]") {
    const Float side = 200._f;
    BlockDomain domain(Vector(side / 2._f), Vector(side));
    RandomDistribution distr(1234);
    Array<Vector> r = distr.generate(SEQUENTIAL, 5000, domain);

    Delaunay delaunay;
    delaunay.build(r);

    const Vector p(50._f, 120._f, 80._f);
    Delaunay::Cell::Handle ch1 = delaunay.locate(p);
    REQUIRE(ch1 != nullptr);
    REQUIRE(delaunay.tetrahedron(*ch1).contains(p));

    REQUIRE(ch1->getNeighborCnt() == 4);
    Delaunay::Cell::Handle ch2 = delaunay.locate(p, ch1);
    Delaunay::Cell::Handle ch3 = delaunay.locate(p, ch1->neighbor(0));
    REQUIRE(ch1 == ch2);
    REQUIRE(ch1 == ch3);

    REQUIRE_SPH_ASSERT(delaunay.locate(Vector(-1._f, 0._f, 0._f)));
}
