#include "utils/Approx.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Approx float", "[approx]") {
    // some sanity checks
    REQUIRE(0._f == approx(0._f));
    REQUIRE(0._f == approx(0._f, 0._f));
    REQUIRE(1._f == approx(1._f));
    REQUIRE(1._f == approx(1._f + EPS));
    REQUIRE_FALSE(1._f == approx(0.5_f));
    REQUIRE_FALSE(0._f == approx(0.5_f));
    REQUIRE(1._f != approx(0.5_f));
    REQUIRE(0._f != approx(0.5_f));

    // test around 1
    REQUIRE(2._f == approx(1.999_f, 1.e-3_f));
    REQUIRE(2._f == approx(2.001_f, 1.e-3_f));
    REQUIRE(2._f != approx(1.999_f, EPS));
    REQUIRE(2._f != approx(2.001_f, EPS));

    // test huge values
    REQUIRE(5.e12_f == approx(4.5e12_f, 0.2_f));
    REQUIRE(5.e12_f == approx(5.5e12_f, 0.2_f));
    REQUIRE(5.e12_f != approx(4.5e12_f, EPS));
    REQUIRE(5.e12_f != approx(5.5e12_f, 0.01_f));

    // test small values
    REQUIRE(3.e-12_f == approx(3.01e-12_f, 1.e-2_f));
    REQUIRE(3.e-12_f == approx(2.99e-12_f, 1.e-2_f));
    REQUIRE(3.e-12_f != approx(0._f, 1.e-12_f));
    REQUIRE(3.e-12_f != approx(0._f, 1.e-12_f));
}

// these are more or less checked by almostEqual test of vector, so test here just few basic cases

TEST_CASE("Approx vector", "[approx]") {
    Vector v1(0.5_f, 2._f, -1._f);
    REQUIRE(v1 == approx(v1));
    REQUIRE(v1 == approx((1._f + EPS) * v1));
    REQUIRE(v1 == approx(Vector(0.5_f, 2._f, -1.01_f), 1e-2_f));
    REQUIRE(v1 != approx(0.5_f * v1));
    REQUIRE(v1 == approx(0.8_f * v1, 0.4_f));
}

TEST_CASE("Approx tensor", "[approx]") {
    SymmetricTensor t1(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    REQUIRE(t1 == approx(t1));
    REQUIRE(t1 == approx((1._f + EPS) * t1));
    REQUIRE(t1 != approx(t1 + SymmetricTensor::identity() * 0.01_f));
    REQUIRE(t1 == approx(t1 + SymmetricTensor::identity() * 0.01_f, 0.01_f));
    REQUIRE(t1 != approx(0.5_f * t1));
    REQUIRE(t1 == approx(0.8_f * t1, 0.4_f));
}

TEST_CASE("Approx traceless tensor", "[approx]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, -5._f, -1._f), Vector(3._f, -1._f, 4._f));
    REQUIRE(t1 == approx(t1));
    REQUIRE(t1 == approx((1._f + EPS) * t1));
    TracelessTensor t2(Vector(1._f, 0._f, 0._f), Vector(0._f, 0._f, 0._f), Vector(0._f, 0._f, -1._f));
    REQUIRE(t1 != approx(t1 + t2 * 0.01_f));
    REQUIRE(t1 == approx(t1 + t2 * 0.01_f, 0.01_f));
    REQUIRE(t1 != approx(0.5_f * t1));
    REQUIRE(t1 == approx(0.8_f * t1, 0.4_f));
}
