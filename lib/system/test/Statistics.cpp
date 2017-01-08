#include "system/Statistics.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Statistics set/get", "[statistics]") {
    FrequentStats stats;
    REQUIRE_FALSE(stats.has(FrequentStatsIds::TIMESTEP_VALUE));
    stats.set(FrequentStatsIds::TIMESTEP_VALUE, 5._f);
    REQUIRE(stats.has(FrequentStatsIds::TIMESTEP_VALUE));
    REQUIRE(stats.get<Float>(FrequentStatsIds::TIMESTEP_VALUE) == 5._f);

    stats.accumulate(FrequentStatsIds::NEIGHBOUR_COUNT, 2._f);
    stats.accumulate(FrequentStatsIds::NEIGHBOUR_COUNT, 7._f);
    stats.accumulate(FrequentStatsIds::NEIGHBOUR_COUNT, 6._f);
    REQUIRE(stats.has(FrequentStatsIds::NEIGHBOUR_COUNT));
    FloatStats f = stats.get<FloatStats>(FrequentStatsIds::NEIGHBOUR_COUNT);
    REQUIRE(f.max() == 7._f);
    REQUIRE(f.min() == 2._f);
    REQUIRE(f.average() == 5._f);
    REQUIRE(f.count() == 3);
}
