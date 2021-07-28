#include "objects/geometry/Triangle.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Triangle basic", "[triangle]") {
    Triangle tr(Vector(0), Vector(1, 0, 0), Vector(0, 1, 0));
    REQUIRE(tr.isValid());
    REQUIRE(tr.area() == 0.5_f);
    REQUIRE(tr.normal() == Vector(0, 0, 1));
    REQUIRE(tr.center() == Vector(1, 1, 0) / 3._f);
    REQUIRE(tr.getBBox() == Box(Vector(0), Vector(1, 1, 0)));
}

TEST_CASE("Triangle invert", "[triangle]") {
    Triangle tr(Vector(1, 3, 4), Vector(-4, 3, -2), Vector(5, 0, 1));
    REQUIRE(tr.inverted().isValid());
    REQUIRE(tr.normal() == -tr.inverted().normal());
}
