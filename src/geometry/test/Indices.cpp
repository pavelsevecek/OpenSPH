#include "geometry/Indices.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Value-value construction", "[indices]") {
    REQUIRE_NOTHROW(Indices is1);
    int i = 3, j = 4, k = 5;
    BasicIndices<int&> is2(i, j, k);
    REQUIRE(is2[0] == 3);
    REQUIRE(is2[1] == 4);
    REQUIRE(is2[2] == 5);
}

TEST_CASE("Reference-reference construction", "[indices]") {
    Indices is1(1, 2, 3);
    BasicIndices<int&> is2(is1);
    BasicIndices<int&> is3(is2);
    REQUIRE(is3 == is1);
}

TEST_CASE("Value-value assignment", "[indices]") {
    Indices is1(1, 2, 3);
    Indices is2;
    is2 = is1;
    REQUIRE(is2 == is1);
}

TEST_CASE("Reference-value assignment", "[indices]") {
    Indices is1(1, 2, 3);
    BasicIndices<int&> is2(is1);
    Indices is3(5, 4, 6);
    is2 = is3;
    REQUIRE(is2 == is3);
}

TEST_CASE("Value-reference assignment", "[indices]") {
    Indices is1(1, 2, 3);
    BasicIndices<int&> is2(is1);
    Indices is3;
    is3 = is2;
    REQUIRE(is3 == is2);
    REQUIRE(is1 == is2);
}

TEST_CASE("Reference-reference assignment", "[indices]") {
    Indices is1(1, 2, 3);
    BasicIndices<int&> is2(is1);
    Indices is3(4, 5, 6);
    BasicIndices<int&> is4(is3);

    is2 = is4;
    REQUIRE(is2 == is4);
    REQUIRE(is2 == is4);
    REQUIRE(is1 != is2);
}

TEST_CASE("Tie indices", "[indices]") {
    Array<Indices> values(0, 3);
    for (int i = 0; i < 3; ++i) {
        values.push(Indices(1, 1, 1));
    }
    auto refs = tieIndices(values[0][0], values[1][1], values[2][2]);
    refs[0]   = 2;
    refs[1]   = 3;
    refs[2]   = 4;

    REQUIRE(values[0] == Indices(2, 1, 1));
    REQUIRE(values[1] == Indices(1, 3, 1));
    REQUIRE(values[2] == Indices(1, 1, 4));
}

TEST_CASE("GetByMultiIndex", "[indices]") {
    Array<Indices> values(0, 3);
    for (int i = 0; i < 3; ++i) {
        values.push(Indices(1, 1, 1));
    }
    auto refs = getByMultiIndex(values, Indices(1, 0, 2));
    refs      = Indices(5, 6, 7);
    REQUIRE(values[0] == Indices(1, 6, 1));
    REQUIRE(values[1] == Indices(5, 1, 1));
    REQUIRE(values[2] == Indices(1, 1, 7));
}

TEST_CASE("GetByMultiIndex 2", "[indices]") {
    Array<Vector> values(0, 3);
    for (int i = 0; i < 3; ++i) {
        values.push(Vector(1.f, 1.f, 1.f));
    }
    auto refs = getByMultiIndex(values, Indices(1, 0, 2));
    refs      = makeIndices(5.f, 6.f, 7.f);
    REQUIRE(values[0] == Vector(1.f, 6.f, 1.f));
    REQUIRE(values[1] == Vector(5.f, 1.f, 1.f));
    REQUIRE(values[2] == Vector(1.f, 1.f, 7.f));
}
