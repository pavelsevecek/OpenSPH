#include "objects/finders/Order.h"
#include "catch.hpp"
#include "math/MathUtils.h"

using namespace Sph;


TEST_CASE("Order shuffle", "[order]") {
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

TEST_CASE("Order compose", "[order]") {
    Order o(5);
    o.shuffle([](int i1, int i2) { return (i1 + 2) % 5 < (i2 + 2) % 5; });
    Order inv = o.getInverted();

    Order composed = inv.compose(o); // composing function and its inversion
    Order id(5);                     // identity
    REQUIRE(id == composed);
}

TEST_CASE("getOrder", "[order]") {
    Array<Float> values{ 1.f, 5.f, 3.f, 2.f, 4.f };
    Order order = getOrder(values);
    REQUIRE(order[0] == 0);
    REQUIRE(order[1] == 4);
    REQUIRE(order[2] == 2);
    REQUIRE(order[3] == 1);
    REQUIRE(order[4] == 3);

    Array<Float> sorted{ 1.f, 2.f, 3.f, 4.f, 5.f };
    REQUIRE(order.apply(sorted) == values);
}
