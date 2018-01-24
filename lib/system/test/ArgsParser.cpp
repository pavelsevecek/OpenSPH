#include "system/ArgsParser.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

enum class TestArgEnum {
    VALUE1,
    VALUE2,
    NAME,
    COUNT,
};

/// ISO C++ forbids converting a string constant to ‘char*’
#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE("No args", "[args]") {
    ArgsParser<TestArgEnum> parser({});
    char* args1[] = { "" };
    // no input args -> no output args
    FlatMap<TestArgEnum, ArgValue> result = parser.parse(1, args1);
    REQUIRE(result.empty());

    char* args2[] = { "", "value" };
    // unknown parameter 'value1'
    REQUIRE_THROWS(parser.parse(2, args2));
}

TEST_CASE("Unnamed args", "[args]") {
    ArgsParser<TestArgEnum> parser({ { TestArgEnum::VALUE1, ArgEnum::FLOAT, OptionalEnum::MANDATORY } });
    char* args1[] = { "", "5.3" };
    FlatMap<TestArgEnum, ArgValue> result = parser.parse(2, args1);
    REQUIRE(result.size() == 1);
    REQUIRE(result[TestArgEnum::VALUE1].get<float>() == approx(5.3, 1.e-6f));

    char* args2[] = { "", "5.3", "4.8" };
    REQUIRE_THROWS(parser.parse(3, args2)); // too many parameters
    REQUIRE_THROWS(parser.parse(1, args1)); // not enough parameters

    char* args3[] = { "", "hello" };
    REQUIRE_THROWS(parser.parse(2, args3)); // invalid parameter
}

TEST_CASE("Unordered unnamed args", "[args]") {
    ArgsParser<TestArgEnum> parser({ { TestArgEnum::COUNT, ArgEnum::INT, OptionalEnum::MANDATORY },
        { TestArgEnum::VALUE2, ArgEnum::FLOAT, OptionalEnum::MANDATORY },
        { TestArgEnum::NAME, ArgEnum::STRING, OptionalEnum::MANDATORY } });
    char* args1[] = { "", "5", "3.3", "test" };
    FlatMap<TestArgEnum, ArgValue> result = parser.parse(4, args1);
    REQUIRE(result.size() == 3);
}

TEST_CASE("Named args", "[args]") {
    ArgsParser<TestArgEnum> parser({ { TestArgEnum::VALUE1, ArgEnum::FLOAT, "-p", "--param" } });
    char* args1[] = { "", "-p", "5.3" };
    FlatMap<TestArgEnum, ArgValue> result = parser.parse(3, args1);
    REQUIRE(result.size() == 1);
    REQUIRE(result[TestArgEnum::VALUE1].get<float>() == approx(5.3, 1.e-6f));

    REQUIRE_THROWS(parser.parse(2, args1));

    char* args2[] = { "", "--param", "4.8" };
    result = parser.parse(3, args2);
    REQUIRE(result.size() == 1);
    REQUIRE(result[TestArgEnum::VALUE1].get<float>() == approx(4.8, 1.e-6f));

    char* args3[] = { "", "-q", "4.8" };
    REQUIRE_THROWS(parser.parse(3, args3));
}

TEST_CASE("Argument list", "[args]") {
    ArgsParser<TestArgEnum> parser({ //
        { TestArgEnum::VALUE1, ArgEnum::FLOAT, "-a" },
        { TestArgEnum::VALUE2, ArgEnum::FLOAT, "-b", "--blong" },
        { TestArgEnum::NAME, ArgEnum::STRING, OptionalEnum::MANDATORY },
        { TestArgEnum::COUNT, ArgEnum::INT, OptionalEnum::OPTIONAL } });
    char* args1[] = { "", "-a", "5.3", "file.txt", "-b", "4.8", "9" };
    FlatMap<TestArgEnum, ArgValue> result = parser.parse(7, args1);
    REQUIRE(result.size() == 4);
    REQUIRE(result[TestArgEnum::VALUE1].get<float>() == approx(5.3, 1.e-6f));
    REQUIRE(result[TestArgEnum::VALUE2].get<float>() == approx(4.8, 1.e-6f));
    REQUIRE(result[TestArgEnum::NAME].get<std::string>() == "file.txt");
    REQUIRE(result[TestArgEnum::COUNT].get<int>() == 9);

    char* args2[] = { "", "--blong", "2.4", "-a", "4.3", "file.txt" };
    result = parser.parse(6, args2);
    REQUIRE(result.size() == 3);
    REQUIRE(result[TestArgEnum::VALUE1].get<float>() == approx(4.3, 1.e-6f));
    REQUIRE(result[TestArgEnum::VALUE2].get<float>() == approx(2.4, 1.e-6f));
    REQUIRE(result[TestArgEnum::NAME].get<std::string>() == "file.txt");
}
