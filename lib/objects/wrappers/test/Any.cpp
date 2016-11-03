#include "objects/wrappers/Any.h"
#include "objects/containers/Array.h"
#include "catch.hpp"
#include <string>

using namespace Sph;


TEST_CASE("Any constructor", "[any]") {
    REQUIRE_NOTHROW(Any a1);
    Any a2 (5);
    Optional<float> f = anyCast<float>(a2);
    REQUIRE(!f);
    Optional<int> i = anyCast<int>(a2);
    REQUIRE(i);
    REQUIRE(i.get() == 5);

    Any a3 (std::string("hello"));
    Optional<std::string> s = anyCast<std::string>(a3);
    REQUIRE(s);
    REQUIRE(s.get() == "hello");
}

TEST_CASE("Any comparisons", "[any]") {
    Any a1 (5.);
    REQUIRE(a1 != 5);   // double != int
    REQUIRE(a1 != 5.f); // double != float
    REQUIRE(a1 != 4);   // 5 != 4
    REQUIRE(a1 == 5.);
}

