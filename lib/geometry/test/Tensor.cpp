#include "geometry/Tensor.h"
#include "objects/containers/Array.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Tensor construction", "[tensor]") {
    REQUIRE_NOTHROW(Tensor t1);

    Tensor t2(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    REQUIRE(t2[0] == Vector(1._f, -1._f, -2._f));
    REQUIRE(t2[1] == Vector(-1._f, 2._f, -3._f));
    REQUIRE(t2[2] == Vector(-2._f, -3._f, 3._f));

    REQUIRE(t2(0, 0) == 1._f);
    REQUIRE(t2(0, 1) == -1._f);
    REQUIRE(t2(0, 2) == -2._f);
    REQUIRE(t2(1, 0) == -1._f);
    REQUIRE(t2(1, 1) == 2._f);
    REQUIRE(t2(1, 2) == -3._f);
    REQUIRE(t2(2, 0) == -2._f);
    REQUIRE(t2(2, 1) == -3._f);
    REQUIRE(t2(2, 2) == 3._f);

    Tensor t3(Vector(1._f, -1._f, -2._f), Vector(-1._f, 2._f, -3._f), Vector(-2._f, -3._f, 3._f));
    REQUIRE(t2 == t3);
}

TEST_CASE("Tensor algebra", "[tensor]") {
    Tensor t(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    REQUIRE(t.determinant() == -26._f);

    const Float detInv = 1._f / 26._f;
    Tensor inv(detInv * Vector(3._f, 1._f, -1._f), detInv*Vector(-9._f, -7._f, -5._f));
    REQUIRE(Math::almostEqual(t.inverse(), inv, EPS));

    Tensor t2(Vector(5._f, 3._f, -3._f), Vector(0._f));
    StaticArray<Float, 3> eigens = findEigenvalues(t2);
    // eigenvalues of diagonal matrix are diagonal elements
    REQUIRE(Math::almostEqual(eigens[0], 5._f));
    REQUIRE(Math::almostEqual(eigens[1], -3._f));
    REQUIRE(Math::almostEqual(eigens[2],  3._f));
}
