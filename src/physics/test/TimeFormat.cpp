#include "physics/TimeFormat.h"
#include "objects/containers/Array.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Julian date", "[dateFormat]") {
    DateFormat dateFormat(2350000.*60*60*24, JulianDateFormat::JD, "%Y:%m:%d - %H:%M:%s");
    /// \todo does not work before Gregorian calendar?
    REQUIRE(dateFormat.get() == "1721:12:24 - 12:00:00");
}

