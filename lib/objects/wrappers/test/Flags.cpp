#include "objects/wrappers/Flags.h"
#include "catch.hpp"

using namespace Sph;

enum Enum { OPT1 = 1 << 0, OPT2 = 1 << 1, OPT3 = 1 << 2 };

TEST_CASE("flags constructor", "[flags]") {
    Flags<Enum> flags;
    flags.set(OPT1);
    REQUIRE(flags.has(OPT1));
    REQUIRE(!flags.has(OPT2));
    REQUIRE(!flags.has(OPT3));

    REQUIRE(flags.hasAny(OPT1, OPT2, OPT3));
}

TEST_CASE("Empty flags", "[flags]") {
    Flags<Enum> flags(OPT1);
    flags = EMPTY_FLAGS;
    REQUIRE(!flags.hasAny(OPT1, OPT2, OPT3));
}