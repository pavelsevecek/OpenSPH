#include "objects/wrappers/Variant.h"
#include "catch.hpp"

using namespace Sph;


TEST_CASE("Variant constructor", "[variant]") {
    Variant<int, float, double> variant1;
    REQUIRE(variant1.getTypeIdx() == -1);

    Variant<int, float> variant2(3.14f);
    REQUIRE(variant2.getTypeIdx() == 1);
    REQUIRE((float)variant2 == 3.14f);

    Variant<int, float> variant3(variant2);
    REQUIRE(variant3.getTypeIdx() == 1);
    REQUIRE((float)variant3 == 3.14f);
}

TEST_CASE("Variant assignemt", "[variant]") {
    Variant<int, float> variant1;
    Variant<int, float> variant2 (5.3f);
    variant1 = variant2;
    REQUIRE(variant1.getTypeIdx() == 1);
    REQUIRE((float)variant1 == 5.3f);

    variant1 = 5;
    REQUIRE(variant1.getTypeIdx() == 0);
    REQUIRE((int)variant1 == 5);
}

TEST_CASE("Variant get", "[variant]") {
    Variant<int, float> variant1;
    REQUIRE(!variant1.get<int>());
    REQUIRE(!variant1.get<float>());
    variant1 = 20;
    REQUIRE(variant1.get<int>() == 20);
    REQUIRE(!variant1.get<float>());
    variant1 = 3.14f;
    REQUIRE(!variant1.get<int>());
    REQUIRE(variant1.get<float>() == 3.14f);
}
