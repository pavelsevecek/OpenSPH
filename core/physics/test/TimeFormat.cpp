#include "physics/TimeFormat.h"
#include "catch.hpp"
#include "objects/containers/Array.h"

using namespace Sph;

TEST_CASE("Julian date", "[date]") {
    DateFormat dateFormat(
        2350000._f * 60._f * 60._f * 24._f + 0.5_f, JulianDateFormat::JD, "%Y:%m:%d - %H:%M:%s");
    /// \todo does not work before Gregorian calendar?
    REQUIRE(dateFormat.get() == "1721:12:24 - 12:00:00");
}
