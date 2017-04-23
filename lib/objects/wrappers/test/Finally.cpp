#include "objects/wrappers/Finally.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Finally", "[finally]") {
    int a = 0;
    {
        auto f = finally([&a] { a = 5; });
        REQUIRE(a == 0);
    }
    REQUIRE(a == 5);
}

/*TEST_CASE("Finally move", "[finally]") {
    int a = 0;
    {
        Finally f1;
        {
            f1 = [&a] { a = 2; };
            // default-constructed finally destroyed here, nothing happens
        }
        REQUIRE(a == 0);

        Finally f2([&a] { a = 5; });

        {
            f1 = std::move(f2);
            // finally with a=2 destroyed here
        }
        REQUIRE(a == 2);
    }
    REQUIRE(a == 5);
}
*/
