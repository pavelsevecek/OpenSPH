#include "objects/utility/StringUtils.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("String split", "[string]") {
    std::string csv = "value1,value2,value3,";
    Array<std::string> parts = split(csv, ',');
    REQUIRE(parts.size() == 4);
    REQUIRE(parts[0] == "value1");
    REQUIRE(parts[1] == "value2");
    REQUIRE(parts[2] == "value3");
    REQUIRE(parts[3] == "");

    parts = split(csv, '/');
    REQUIRE(parts.size() == 1);
    REQUIRE(parts[0] == csv);
}

TEST_CASE("fromString", "[string]") {
    Optional<int> o1 = fromString<int>("53");
    REQUIRE(o1.value() == 53);
}
