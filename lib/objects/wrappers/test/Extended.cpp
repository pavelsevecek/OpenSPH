#include "objects/wrappers/Extended.h"
#include "catch.hpp"

using namespace Sph;


TEST_CASE("Extended constructor", "[extended]") {
    Extended e1(5._f);
    REQUIRE(e1.isFinite());
    Extended e2(Extended::infinity());
    REQUIRE(!e2.isFinite());
    Extended e3(-Extended::infinity());
    REQUIRE(!e3.isFinite());

    REQUIRE((Float)e1 == 5._f);
}

TEST_CASE("Extended comparisons", "[extended]") {
    Extended e1(5._f);
    Extended e2(Extended::infinity());
    Extended e3(-Extended::infinity());

    REQUIRE(e1 != e2);
    REQUIRE(e1 != e3);
    REQUIRE(e2 != e3);
    REQUIRE(e1 == 5._f);
    REQUIRE(e2 == Extended::infinity());
    REQUIRE(-e2 == -Extended::infinity());
    REQUIRE(-e3 == Extended::infinity());
    REQUIRE(e3 == -Extended::infinity());

    REQUIRE(e2 > e1);
    REQUIRE(e1 < e2);
    REQUIRE(e3 < e1);
    REQUIRE(e1 > e3);
    REQUIRE(e2 > e3);
    REQUIRE(e3 < e2);

    REQUIRE(e1 > 4._f);
    REQUIRE(e1 < 6._f);
    REQUIRE(e1 < Extended::infinity());
    REQUIRE(e1 > -Extended::infinity());
    REQUIRE_FALSE(e2 < Extended::infinity());
    REQUIRE(e2 > -Extended::infinity());
    REQUIRE_FALSE(e3 > -Extended::infinity());
    REQUIRE(e3 < Extended::infinity());


    REQUIRE(e2 >= e1);
    REQUIRE(e1 <= e2);
    REQUIRE(e3 <= e1);
    REQUIRE(e1 >= e3);
    REQUIRE(e2 >= e3);
    REQUIRE(e3 <= e2);

    REQUIRE(e1 >= 4._f);
    REQUIRE(e1 >= 5._f);
    REQUIRE(e1 <= 6._f);
    REQUIRE(e1 <= 5._f);
    REQUIRE(e1 <= Extended::infinity());
    REQUIRE(e1 >= -Extended::infinity());

    REQUIRE(e2 <= Extended::infinity());
    REQUIRE(e2 >= Extended::infinity());
    REQUIRE(e2 >= -Extended::infinity());
    REQUIRE(e3 >= -Extended::infinity());
    REQUIRE(e3 <= -Extended::infinity());
    REQUIRE(e3 <= Extended::infinity());
}

TEST_CASE("Extended operations", "[extended]") {
    Extended e1(6._f);
    Extended e2(-3._f);
    Extended plusinf(Extended::infinity());
    Extended minusinf(-Extended::infinity());

    REQUIRE(e1 + e2 == 3._f);
    REQUIRE(e1 - e2 == 9._f);
    REQUIRE(e1 + plusinf == plusinf);
    REQUIRE(e1 + minusinf == minusinf);
    REQUIRE(e1 - plusinf == minusinf);
    REQUIRE(plusinf - e1 == plusinf);
    REQUIRE(e1 - minusinf == plusinf);
    REQUIRE(minusinf - e1 == minusinf);

    REQUIRE(e1 * e2 == -18._f);
    REQUIRE(e1 * plusinf == plusinf);
    REQUIRE(e1 * minusinf == minusinf);
    REQUIRE(e2 * plusinf == minusinf);
    REQUIRE(e2 * minusinf == plusinf);
    REQUIRE(plusinf * e1 == plusinf);
    REQUIRE(minusinf * e1 == minusinf);
    REQUIRE(plusinf * e2 == minusinf);
    REQUIRE(minusinf * e2 == plusinf);
    REQUIRE(plusinf * plusinf == plusinf);
    REQUIRE(plusinf * minusinf == minusinf);
    REQUIRE(minusinf * plusinf == minusinf);
    REQUIRE(minusinf * minusinf == plusinf);

    REQUIRE(e1 / e2 == -2._f);
    REQUIRE(e1 / plusinf == 0._f);
    REQUIRE(e1 / minusinf == 0._f);
    REQUIRE(e2 / plusinf == 0._f);
    REQUIRE(e2 / minusinf == 0._f);
    REQUIRE(plusinf / e1 == plusinf);
    REQUIRE(plusinf / e2 == minusinf);
    REQUIRE(minusinf / e1 == minusinf);
    REQUIRE(minusinf / e2 == plusinf);
}

TEST_CASE("Extended sign", "[sign]") {
    REQUIRE(Extended(5._f).sign() == 1);
    REQUIRE(Extended(-3._f).sign() == -1);
    REQUIRE(Extended::infinity().sign() == 1);
    REQUIRE(-Extended::infinity().sign() == -1);
}
