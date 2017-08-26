#include "math/Roots.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("GetRoots", "[roots]") {
    Optional<Float> root = getRoot([](const Float x) { return cos(x); }, Interval(0._f, PI));
    REQUIRE(root);
    REQUIRE(root.value() == approx(0.5_f * PI));

    REQUIRE_FALSE(getRoot([](const Float) { return 1._f; }, Interval(0._f, 1._f)));
}
