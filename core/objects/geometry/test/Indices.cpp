#include "objects/geometry/Indices.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Indices construction", "[indices]") {
    Indices i1(1, 2, 3, 4);
    REQUIRE(i1[0] == 1);
    REQUIRE(i1[1] == 2);
    REQUIRE(i1[2] == 3);
    REQUIRE(i1[3] == 4);

    Indices i2(5);
    REQUIRE(i2[0] == 5);
    REQUIRE(i2[1] == 5);
    REQUIRE(i2[2] == 5);
    REQUIRE(i2[3] == 5);

    Indices i3(i1);
    REQUIRE(i3[0] == 1);
    REQUIRE(i3[1] == 2);
    REQUIRE(i3[2] == 3);
    REQUIRE(i3[3] == 4);
}

TEST_CASE("Indices comparison", "[indices]") {
    Indices i1(1, 2, 3, 5);
    Indices i2(1, 2, 3, 7);
    Indices i3(1, -1, 3, 5);
    Indices i12 = i1 == i2;
    Indices i13 = i1 == i3;

    REQUIRE(i12[0]);
    REQUIRE(i12[1]);
    REQUIRE(i12[2]);
    REQUIRE_FALSE(i12[3]);

    REQUIRE(i13[0]);
    REQUIRE_FALSE(i13[1]);
    REQUIRE(i13[2]);
    REQUIRE(i13[3]);

    Indices ni12 = i1 != i2;
    Indices ni13 = i1 != i3;

    REQUIRE_FALSE(ni12[0]);
    REQUIRE_FALSE(ni12[1]);
    REQUIRE_FALSE(ni12[2]);
    REQUIRE(ni12[3]);

    REQUIRE_FALSE(ni13[0]);
    REQUIRE(ni13[1]);
    REQUIRE_FALSE(ni13[2]);
    REQUIRE_FALSE(ni13[3]);
}

TEST_CASE("Indices conversion", "[indices]") {
    Vector v(1.5_f, 2.4_f, 5._f);
    Indices i(v);

    REQUIRE(i[0] == 1);
    REQUIRE(i[1] == 2);
    REQUIRE(i[2] == 5);

    Vector v2 = i;
    REQUIRE(v2 == Vector(1._f, 2._f, 5._f));
}

TEST_CASE("Indices random access", "[indices]") {
    Indices i(4, 5, 6);
    i[2]++;
    REQUIRE(all(i == Indices(4, 5, 7)));
    REQUIRE(i[0] == 4);
    REQUIRE(i[1] == 5);
    REQUIRE(i[2] == 7);
    REQUIRE(i[3] == 0);

    i[1]--;
    REQUIRE(all(i == Indices(4, 4, 7)));
    REQUIRE(i[0] == 4);
    REQUIRE(i[1] == 4);
    REQUIRE(i[2] == 7);
    REQUIRE(i[3] == 0);

    REQUIRE_SPH_ASSERT(i[4]);
}

TEST_CASE("Indices arithmetics", "[indices]") {
    Indices i1(2, 4, 3);
    Indices i2(5, -1, 2);
    REQUIRE(all(i1 + i2 == Indices(7, 3, 5)));
    REQUIRE(all(i1 - i2 == Indices(-3, 5, 1)));
    REQUIRE(all(i1.max(i2) == Indices(5, 4, 3)));
    REQUIRE(all(i1.min(i2) == Indices(2, -1, 2)));
}
