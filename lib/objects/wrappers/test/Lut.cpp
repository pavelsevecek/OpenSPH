#include "objects/wrappers/Lut.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Lut", "[lut]") {
    Lut<float> lut(Interval(0._f, 2._f * PI), 10000, [](const Float x) { return sin(x); });

    REQUIRE(lut(10._f) == approx(0._f));
    REQUIRE(lut(-10._f) == approx(0._f));

    for (Float x = 0._f; x <= 2._f * PI; x += 0.5_f) {
        REQUIRE(lut(x) == approx(sin(x), 1.e-5_f));
    }
}
