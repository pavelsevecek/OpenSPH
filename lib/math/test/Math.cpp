#include "math/Math.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Math", "[math]") {
    REQUIRE(almostEqual(sqrtApprox(100.f), 10.f, 0.2f));

    REQUIRE(pow<0>(2._f) == 1._f);
    REQUIRE(pow<1>(2._f) == 2._f);
    REQUIRE(pow<2>(2._f) == 4._f);
    REQUIRE(pow<3>(2._f) == 8._f);
    REQUIRE(pow<4>(2._f) == 16._f);
    REQUIRE(pow<5>(2._f) == 32._f);
    REQUIRE(pow<6>(2._f) == 64._f);

    REQUIRE(max(6, 2) == 6);
    REQUIRE(min(6, 2) == 2);

    REQUIRE(max(1, 2, 3) == 3);
    REQUIRE(max(1, 3, 2) == 3);
    REQUIRE(max(2, 1, 3) == 3);
    REQUIRE(max(2, 3, 1) == 3);
    REQUIRE(max(3, 1, 2) == 3);
    REQUIRE(max(3, 2, 1) == 3);

    REQUIRE(min(1, 2, 3) == 1);
    REQUIRE(min(1, 3, 2) == 1);
    REQUIRE(min(2, 1, 3) == 1);
    REQUIRE(min(2, 3, 1) == 1);
    REQUIRE(min(3, 1, 2) == 1);
    REQUIRE(min(3, 2, 1) == 1);

    REQUIRE(clamp(-1, 2, 4) == 2);
    REQUIRE(clamp(3, 2, 4) == 3);
    REQUIRE(clamp(5, 2, 4) == 4);

    REQUIRE(isPower2(2));
    REQUIRE(isPower2(4));
    REQUIRE(isPower2(32));
    REQUIRE_FALSE(isPower2(31));
    REQUIRE_FALSE(isPower2(33));
    REQUIRE_FALSE(isPower2(7));
}

TEST_CASE("Rounding mode", "[math]") {
    // this is a little sandbox, doesn't really belong to tests
    REQUIRE(int(5.3f) == 5);
    REQUIRE(int(5.7f) == 5);
    REQUIRE(int(5.95f) == 5);
    REQUIRE(int(-5.3f) == -5);
    REQUIRE(int(-5.5f) == -5);
    REQUIRE(int(-5.8f) == -5);
}
