#include "math/Math.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Math", "[math]") {
    REQUIRE(Math::almostEqual(Math::sqrtApprox(100.f), 10.f, 0.2f));
}
