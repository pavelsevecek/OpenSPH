#include "system/Statistics.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Statistics set/get", "[statistics]") {
    Statistics stats;
    REQUIRE_FALSE(stats.has(StatisticsIds::TIMESTEP_VALUE));
    stats.set(StatisticsIds::TIMESTEP_VALUE, 5._f);
    REQUIRE(stats.has(StatisticsIds::TIMESTEP_VALUE));
    REQUIRE(stats.get<Float>(StatisticsIds::TIMESTEP_VALUE) == 5._f);

    stats.accumulate(StatisticsIds::NEIGHBOUR_COUNT, 2._f);
    stats.accumulate(StatisticsIds::NEIGHBOUR_COUNT, 7._f);
    stats.accumulate(StatisticsIds::NEIGHBOUR_COUNT, 6._f);
    REQUIRE(stats.has(StatisticsIds::NEIGHBOUR_COUNT));
    Means f = stats.get<Means>(StatisticsIds::NEIGHBOUR_COUNT);
    REQUIRE(f.max() == 7._f);
    REQUIRE(f.min() == 2._f);
    REQUIRE(f.average() == 5._f);
    REQUIRE(f.count() == 3);
}
