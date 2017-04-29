#include "objects/wrappers/Flags.h"
#include "catch.hpp"

using namespace Sph;

enum Enum { OPT1 = 1 << 0, OPT2 = 1 << 1, OPT3 = 1 << 2 };

TEST_CASE("Flags constructor", "[flags]") {
    Flags<Enum> flags;
    flags.set(OPT1);
    REQUIRE(flags.has(OPT1));
    REQUIRE_FALSE(flags.has(OPT2));
    REQUIRE_FALSE(flags.has(OPT3));

    REQUIRE(flags.hasAny(OPT1, OPT2, OPT3));
    REQUIRE_FALSE(flags.hasAll(OPT1, OPT2, OPT3));

    flags.set(OPT3);
    REQUIRE(flags.has(OPT1));
    REQUIRE_FALSE(flags.has(OPT2));
    REQUIRE(flags.has(OPT3));
    REQUIRE_FALSE(flags.hasAll(OPT1, OPT2, OPT3));
    REQUIRE(flags.hasAll(OPT1, OPT3));

    flags.unset(OPT1);
    REQUIRE(!flags.has(OPT1));
    REQUIRE(!flags.has(OPT2));
    REQUIRE(flags.has(OPT3));

    flags.setIf(OPT1, true);
    flags.setIf(OPT2, true);
    flags.setIf(OPT3, false);
    REQUIRE(flags.has(OPT1));
    REQUIRE(flags.has(OPT2));
    REQUIRE(!flags.has(OPT3));
}

TEST_CASE("Flags operator", "[flags]") {
    Flags<Enum> flags;
    flags = OPT1 | OPT2;
    REQUIRE(flags.has(OPT1));
    REQUIRE(flags.has(OPT2));
    REQUIRE(!flags.has(OPT3));
    flags = OPT1 | OPT2 | OPT3;
    REQUIRE(flags.has(OPT1));
    REQUIRE(flags.has(OPT2));
    REQUIRE(flags.has(OPT3));
}

TEST_CASE("Empty flags", "[flags]") {
    Flags<Enum> flags(OPT1);
    flags = EMPTY_FLAGS;
    REQUIRE(!flags.hasAny(OPT1, OPT2, OPT3));
}
