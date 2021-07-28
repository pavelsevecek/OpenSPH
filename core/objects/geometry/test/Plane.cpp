#include "objects/geometry/Plane.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Plane signed distance", "[plane]") {
    Plane p(Vector(1, 0, 1), Vector(1, 0, 0));
    REQUIRE(p.signedDistance(Vector(1, 0, 1)) == 0._f);
    REQUIRE(p.signedDistance(Vector(1, 0, 0)) == 0._f);
    REQUIRE(p.signedDistance(Vector(2, 0, 0)) == 1._f);
    REQUIRE(p.signedDistance(Vector(0, -1, -2)) == -1._f);

    REQUIRE(p.above(Vector(1.5_f, 0, 0)));
}

TEST_CASE("Plane from triangle", "[plane]") {
    Triangle tri(Vector(0, 0, 0), Vector(0, 1, 0), Vector(1, 0, 0));
    Plane p(tri);
    REQUIRE(p.normal() == Vector(0, 0, -1));
    for (Size i = 0; i < 3; ++i) {
        REQUIRE(p.signedDistance(tri[i]) == 0._f);
    }
}

TEST_CASE("Plane intersection", "[plane]") {
    Triangle tri(Vector(0, 1, 0), Vector(1, 1, 0), Vector(0, 1, 1));
    Plane p(tri);
    SPH_ASSERT(p.normal() == approx(Vector(0, -1, 0)));
    Vector is = p.intersection(Vector(0), getNormalized(Vector(1, 1, 2)));
    REQUIRE(is == approx(Vector(1, 1, 2)));

    const Vector origin(3, -2, 4);
    const Vector dir(-3.5, -1, 1);
    is = p.intersection(origin, getNormalized(dir));
    REQUIRE(is == approx(origin - 3 * dir));

    REQUIRE_SPH_ASSERT(p.intersection(Vector(0._f), Vector(0, 0, 1)));
}
