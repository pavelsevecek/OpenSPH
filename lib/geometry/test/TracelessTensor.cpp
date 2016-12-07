#include "geometry/TracelessTensor.h"
#include "objects/containers/Array.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("TracelessTensor construction", "[tracelesstesnro]") {
    REQUIRE_NOTHROW(TracelessTensor t1);

    Tensor t2(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    REQUIRE(t2[0] == Vector(1._f, 2._f, 3._f));
    REQUIRE(t2[1] == Vector(2._f, 2._f, 4._f));
    REQUIRE(t2[2] == Vector(3._f, 4._f, -3._f));

    REQUIRE(t2(0, 0) == 1._f);
    REQUIRE(t2(0, 1) == 2._f);
    REQUIRE(t2(0, 2) == 3._f);
    REQUIRE(t2(1, 0) == 2._f);
    REQUIRE(t2(1, 1) == 2._f);
    REQUIRE(t2(1, 2) == 4._f);
    REQUIRE(t2(2, 0) == 3._f);
    REQUIRE(t2(2, 1) == 4._f);
    REQUIRE(t2(2, 2) == -3._f);
}

TEST_CASE("TracelessTensor apply", "[tensor]") {
    Tensor t(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    Vector v(2._f, 1._f, -1._f);
    REQUIRE(t * v == Vector(1._f, 2._f, 13._f));
    v = Vector(0._f);
    REQUIRE(t * v == Vector(0._f));
 }
