#include "geometry/Tensor.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Tensor matrix multiplication", "[tensor]") {
    Tensor t1(Vector(1._f, 2._f, -1._f), Vector(0._f, -3._f, 1._f), Vector(2._f, 4._f, -5._f));
    Tensor t2(Vector(0._f, -1._f, 5._f), Vector(2._f, 3._f, 7._f), Vector(-1._f, -4._f, 1._f));
    Tensor expected(Vector(5._f, 9._f, 18._f), Vector(-7._f, -13._f, -20._f), Vector(13._f, 30._f, 33._f));
    REQUIRE(t1 * t2 == expected);
}

TEST_CASE("Tensor inverse", "[tensor]") {}
