#include "system/ArgsParser.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

/// ISO C++ forbids converting a string constant to ‘char*’
#pragma GCC diagnostic ignored "-Wwrite-strings"

TEST_CASE("ArgParser No args", "[args]") {
    ArgParser parser({});
    char* args1[] = { "" };
    // no input args -> no output args
    REQUIRE_NOTHROW(parser.parse(1, args1));
    REQUIRE(parser.size() == 0);

    char* args2[] = { "", "value" };
    // unknown parameter 'value1'
    REQUIRE_THROWS(parser.parse(2, args2));
}

TEST_CASE("ArgParser getArg", "[args]") {
    ArgParser parser(Array<ArgDesc>{ { "p", "param", ArgEnum::FLOAT, "" } });
    char* args1[] = { "", "-p", "5.3" };
    REQUIRE_NOTHROW(parser.parse(3, args1));
    REQUIRE(parser.size() == 1);
    REQUIRE(parser.getArg<Float>("p") == approx(5.3, 1.e-6f));
    REQUIRE_THROWS(parser.getArg<int>("p"));
    REQUIRE_THROWS(parser.getArg<Float>("r"));

    char* args2[] = { "", "--param", "4.8" };
    REQUIRE_NOTHROW(parser.parse(3, args2));
    REQUIRE(parser.size() == 1);
    REQUIRE(parser.getArg<Float>("p") == approx(4.8, 1.e-6f));

    char* args3[] = { "", "-q", "4.8" };
    REQUIRE_THROWS(parser.parse(3, args3));
}

TEST_CASE("ArgParser tryGetArg", "[args]") {
    ArgParser parser(Array<ArgDesc>{
        { "v", "value", ArgEnum::FLOAT, "" },
        { "n", "number", ArgEnum::INT, "" },
    });
    char* args[] = { "", "-n", "5" };
    REQUIRE_NOTHROW(parser.parse(3, args));

    REQUIRE(parser.size() == 1);
    REQUIRE(parser.tryGetArg<int>("n"));
    REQUIRE(parser.tryGetArg<int>("n") == 5);
    REQUIRE_FALSE(parser.tryGetArg<Float>("v"));

    REQUIRE_THROWS(parser.tryGetArg<Float>("n"));
    REQUIRE_THROWS(parser.tryGetArg<Float>("qq"));
}

TEST_CASE("ArgParser store", "[args]") {
    ArgParser parser(Array<ArgDesc>{
        { "v", "value", ArgEnum::FLOAT, "" },
        { "n", "number", ArgEnum::INT, "" },
        { "o", "other", ArgEnum::INT, "" },
    });
    char* args[] = { "", "-n", "5", "--value", "3.14" };
    REQUIRE_NOTHROW(parser.parse(5, args));

    BodySettings settings = EMPTY_SETTINGS;
    REQUIRE(parser.tryStore(settings, "n", BodySettingsId::PARTICLE_COUNT));
    REQUIRE(parser.tryStore(settings, "v", BodySettingsId::DENSITY));
    REQUIRE_FALSE(parser.tryStore(settings, "o", BodySettingsId::MIN_PARTICLE_COUNT));
    REQUIRE_THROWS(parser.tryStore(settings, "qq", BodySettingsId::BODY_CENTER));

    REQUIRE(settings.size() == 2);
    REQUIRE(settings.get<Float>(BodySettingsId::DENSITY) == approx(3.14_f, 1.e-6_f));
    REQUIRE(settings.get<int>(BodySettingsId::PARTICLE_COUNT) == 5);
}
