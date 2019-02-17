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

TEST_CASE("String replaceAll", "[string]") {
    REQUIRE(replaceAll("test", "1", "grr") == "test");
    std::string s1 = "test 1 of 1 replace 1 all";
    std::string s2 = replaceAll(s1, "1", "2");
    REQUIRE(s2 == "test 2 of 2 replace 2 all");
    s2 = replaceAll(s1, "1", "dummy");
    REQUIRE(s2 == "test dummy of dummy replace dummy all");
    s2 = replaceAll(s1, "1", "111");
    REQUIRE(s2 == "test 111 of 111 replace 111 all");
}

TEST_CASE("String lineBreak", "[string]") {
    REQUIRE(setLineBreak("test test", 6) == "test\ntest");
    REQUIRE(setLineBreak("test, test", 10) == "test, test");
    REQUIRE(setLineBreak("test, test", 4) == "test,\ntest");
    REQUIRE(setLineBreak("test, test", 5) == "test,\ntest");
    REQUIRE(setLineBreak("test, test", 6) == "test,\ntest");

    REQUIRE(
        setLineBreak("- option1: test test test test", 22) == "- option1: test test\n           test test");
}
