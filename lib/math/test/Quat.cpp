#include "math/Quat.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Quaternion rotation", "[quat]") {
    Quat q1(Vector(1._f, 0._f, 0._f), 0.35_f);
    REQUIRE(q1.convert() == approx(AffineMatrix::rotateX(0.35_f)));

    Vector axis = getNormalized(Vector(3._f, -2._f, 1._f));
    Quat q2(axis, 0.2_f);
    REQUIRE(q2.convert() == approx(AffineMatrix::rotateAxis(axis, 0.2_f)));
}

TEST_CASE("Quaternion roundtrip", "[quat]") {
    AffineMatrix m = AffineMatrix::rotateAxis(getNormalized(Vector(-4._f, 3._f, 2._f)), 0.5_f);
    REQUIRE(Quat(m).convert() == approx(m));
}

TEST_CASE("Quaternion axis and angle", "[quat]") {
    const Vector axis = getNormalized(Vector(3._f, -1._f, 2._f));
    const Float angle = 0.25_f;
    Quat q(axis, angle);
    REQUIRE(q.angle() == approx(angle));
    REQUIRE(q.axis() == approx(axis));
}
