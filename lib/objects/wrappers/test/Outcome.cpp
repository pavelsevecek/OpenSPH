#include "objects/wrappers/Outcome.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Oucome success", "[outcome]") {
    Outcome o1(SUCCESS);
    REQUIRE(o1);
    REQUIRE_FALSE(!o1);
    REQUIRE(o1.success());
    REQUIRE_ASSERT(o1.error());

    Outcome o2(true);
    REQUIRE(o2);

    static_assert(
        !std::is_default_constructible<Outcome>::value, "Outcome shall not be default constructible");
}

TEST_CASE("Outcome fail", "[outcome]") {
    Outcome o1(FailTag{});
    REQUIRE(!o1);
    REQUIRE_FALSE(o1);
    REQUIRE_FALSE(o1.success());
    REQUIRE(o1.error() == "error");

    Outcome o2(false);
    REQUIRE(!o2);
    REQUIRE(o2.error() == "error");

    Outcome o3("error message");
    REQUIRE(!o3);
    REQUIRE(o3.error() == "error message");
}

TEST_CASE("Outcome copy/move", "[outcome]") {
    Outcome o1(true);
    Outcome o2(o1);
    REQUIRE(o2);

    Outcome o3("error message");
    Outcome o4(o3);
    REQUIRE(!o4);
    REQUIRE(o4.error() == "error message");

    Outcome o5(true);
    o5 = "error2";
    REQUIRE(!o5);
    REQUIRE(o5.error() == "error2");
}

TEST_CASE("makeFailed", "[outcome]") {
    Outcome o = makeFailed("error", 5, 'x');
    REQUIRE(!o);
    REQUIRE(o.error() == "error5x");
}
