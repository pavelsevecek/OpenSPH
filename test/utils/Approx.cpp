#include "catch.hpp"
#include "utils/Approx.h"

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
    REQUIRE(3.e-12_f == approx(3.01e-12_f, 1e-2_f));
    REQUIRE(3.e-12_f == approx(2.99e-12_f, 1e-2_f));
    REQUIRE(3.e-12_f != approx(3.01e-12_f, 1e-4_f));
    REQUIRE(3.e-12_f != approx(2.99e-12_f, 1e-4_f));
}
