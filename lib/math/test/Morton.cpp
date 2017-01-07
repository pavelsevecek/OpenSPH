#include "math/Morton.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Morton", "[morton]") {
    REQUIRE(morton(Vector(0._f)) == 0);
    REQUIRE(morton(Vector(0._f, 0._f, 1._f / 1024._f)) == 1);
    REQUIRE(morton(Vector(0._f, 1._f / 1024._f, 0._f)) == 2);
    REQUIRE(morton(Vector(0._f, 1._f / 1024._f, 1._f / 1024._f)) == 3);
    REQUIRE(morton(Vector(1._f / 1024._f, 0._f, 0._f)) == 4);
    REQUIRE(morton(Vector(1._f - EPS)) == Math::pow<3>(Size(1024)) - 1);
}
