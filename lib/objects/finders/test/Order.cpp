#include "objects/finders/Order.h"
#include "catch.hpp"
#include "math/Math.h"

using namespace Sph;


TEST_CASE("Shuffle", "[order]") {
    Order o(5);
    o.shuffle([](int i1, int i2) { return (i1 + 2) % 5 < (i2 + 2) % 5; });
    Array<int> expected = { 3, 4, 0, 1, 2 };
    for (int i = 0; i < 5; ++i) {
        REQUIRE(o[i] == expected[i]);
    }
    Order inv = o.getInverted();
    Array<int> invExpected = { 2, 3, 4, 0, 1 };
    for (int i = 0; i < 5; ++i) {
        REQUIRE(inv[i] == invExpected[i]);
    }
    REQUIRE(inv.getInverted() == o);
}

TEST_CASE("Compose", "[order]") {
    Order o(5);
    o.shuffle([](int i1, int i2) { return (i1 + 2) % 5 < (i2 + 2) % 5; });
    Order inv = o.getInverted();

    Order composed = inv(o); // composing function and its inversion
    Order id(5);             // identity
    REQUIRE(id == composed);
}

TEST_CASE("Vector order", "[order]") {
    VectorOrder o(5);
    o.shuffle(1, [](int i1, int i2) { return (i1 + 2) % 5 < (i2 + 2) % 5; });
    Array<Indices> expected = {
        Indices(0, 3, 0), Indices(1, 4, 1), Indices(2, 0, 2), Indices(3, 1, 3), Indices(4, 2, 4)
    };
    for (int i = 0; i < 5; ++i) {
        REQUIRE(o[i][0] == expected[i][0]);
        REQUIRE(o[i][1] == expected[i][1]);
        REQUIRE(o[i][2] == expected[i][2]);
    }
}
