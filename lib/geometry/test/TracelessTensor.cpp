#include "geometry/TracelessTensor.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("TracelessTensor construction", "[tracelesstensor]") {
    REQUIRE_NOTHROW(TracelessTensor t1);

    TracelessTensor t2(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
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

TEST_CASE("TracelessTensor copy", "[tracelesstensor]") {
    /// \todo copy construct from TracelessTensor and Tensor
}

TEST_CASE("TracelessTensor apply", "[tracelesstensor]") {
    TracelessTensor t(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    Vector v(2._f, 1._f, -1._f);
    REQUIRE(t * v == Vector(1._f, 2._f, 13._f));
    v = Vector(0._f);
    REQUIRE(t * v == Vector(0._f));
}

TEST_CASE("TracelessTensor diagonal", "[tracelesstensor]") {
    TracelessTensor t1(5._f);
    REQUIRE(t1.diagonal() == Vector(5._f, 5._f, -10._f));
    REQUIRE(t1.offDiagonal() == Vector(5._f, 5._f, 5._f));
    TracelessTensor t2(Vector(1._f, 0._f, -1._f), Vector(0._f, 4._f, 6._f), Vector(-1._f, 6._f, -5._f));
    REQUIRE(t2.diagonal() == Vector(1._f, 4._f, -5._f));
    REQUIRE(t2.offDiagonal() == Vector(0._f, -1._f, 6._f));
}

TEST_CASE("TracelessTensor double-dot", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    TracelessTensor t2(Vector(-1._f, 0._f, 1._f), Vector(0._f, -2._f, 1._f), Vector(1._f, 1._f, 3._f));
    REQUIRE(ddot(t1, t2) == 0._f);

    Tensor t3(Vector(2._f, -1._f, 0._f), Vector(-1._f, 4._f, 3._f), Vector(0._f, 3._f, -2._f));
    REQUIRE(ddot(t1, t3) == 36._f);
}
