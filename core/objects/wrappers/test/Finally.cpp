#include "objects/wrappers/Finally.h"
#include "catch.hpp"
#include "objects/wrappers/Function.h"

using namespace Sph;

TEST_CASE("Finally", "[finally]") {
    int a = 0;
    {
        auto f = finally([&a] { a++; });
        REQUIRE(a == 0);
    }
    REQUIRE(a == 1);
}
