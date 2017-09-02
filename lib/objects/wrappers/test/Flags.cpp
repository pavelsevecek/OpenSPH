#include "objects/wrappers/Flags.h"
#include "catch.hpp"

using namespace Sph;

enum class Enum { OPT1 = 1 << 0, OPT2 = 1 << 1, OPT3 = 1 << 2 };

TEST_CASE("Flags constructor", "[flags]") {
    Flags<Enum> flags;
    flags.set(Enum::OPT1);
    REQUIRE(flags.has(Enum::OPT1));
    REQUIRE_FALSE(flags.has(Enum::OPT2));
    REQUIRE_FALSE(flags.has(Enum::OPT3));

    REQUIRE(flags.hasAny(Enum::OPT1, Enum::OPT2, Enum::OPT3));
    REQUIRE_FALSE(flags.hasAll(Enum::OPT1, Enum::OPT2, Enum::OPT3));

    flags.set(Enum::OPT3);
    REQUIRE(flags.has(Enum::OPT1));
    REQUIRE_FALSE(flags.has(Enum::OPT2));
    REQUIRE(flags.has(Enum::OPT3));
    REQUIRE_FALSE(flags.hasAll(Enum::OPT1, Enum::OPT2, Enum::OPT3));
    REQUIRE(flags.hasAll(Enum::OPT1, Enum::OPT3));

    flags.unset(Enum::OPT1);
    REQUIRE(!flags.has(Enum::OPT1));
    REQUIRE(!flags.has(Enum::OPT2));
    REQUIRE(flags.has(Enum::OPT3));

    flags.setIf(Enum::OPT1, true);
    flags.setIf(Enum::OPT2, true);
    flags.setIf(Enum::OPT3, false);
    REQUIRE(flags.has(Enum::OPT1));
    REQUIRE(flags.has(Enum::OPT2));
    REQUIRE(!flags.has(Enum::OPT3));
}

TEST_CASE("Flags operator", "[flags]") {
    Flags<Enum> flags;
    flags = Enum::OPT1 | Enum::OPT2;
    REQUIRE(flags.has(Enum::OPT1));
    REQUIRE(flags.has(Enum::OPT2));
    REQUIRE(!flags.has(Enum::OPT3));
    flags = Enum::OPT1 | Enum::OPT2 | Enum::OPT3;
    REQUIRE(flags.has(Enum::OPT1));
    REQUIRE(flags.has(Enum::OPT2));
    REQUIRE(flags.has(Enum::OPT3));
}

TEST_CASE("Empty flags", "[flags]") {
    Flags<Enum> flags(Enum::OPT1);
    flags = EMPTY_FLAGS;
    REQUIRE(!flags.hasAny(Enum::OPT1, Enum::OPT2, Enum::OPT3));
}
