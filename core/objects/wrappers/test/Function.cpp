#include "objects/wrappers/Function.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Function default construct", "[function]") {
    Function<void(int)> func;
    REQUIRE_FALSE(func);
    REQUIRE_SPH_ASSERT(func(5));
}

TEST_CASE("Function functor construct", "[function]") {
    Function<int(int)> func([](const int i) { return i + 1; });

    REQUIRE(func);
    REQUIRE(func(5) == 6);
}

TEST_CASE("Function functor assign", "[function]") {
    Function<int(int)> func;
    func = [](const int i) { return i + 1; };
    REQUIRE(func);
    REQUIRE(func(5) == 6);
}
