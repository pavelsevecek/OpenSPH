#include "math/Math.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Math", "[math]") {
    REQUIRE(Math::almostEqual(Math::sqrtApprox(100.f), 10.f, 0.2f));

    REQUIRE(Math::pow<0>(2._f) == 1._f);
    REQUIRE(Math::pow<1>(2._f) == 2._f);
    REQUIRE(Math::pow<2>(2._f) == 4._f);
    REQUIRE(Math::pow<3>(2._f) == 8._f);
    REQUIRE(Math::pow<4>(2._f) == 16._f);
    REQUIRE(Math::pow<5>(2._f) == 32._f);
    REQUIRE(Math::pow<6>(2._f) == 64._f);

}
