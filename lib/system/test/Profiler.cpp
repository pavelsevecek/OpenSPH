#include "system/Profiler.h"
#include "catch.hpp"
#include <thread>

using namespace Sph;

void function1() {
    PROFILE_SCOPE("function1");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void function2() {
    PROFILE_SCOPE("function2");
    std::this_thread::sleep_for(std::chrono::milliseconds(70));
}

TEST_CASE("Profiler", "[profiler]") {
    Profiler::getInstance()->clear();
    {
        PROFILE_SCOPE("all");
        function1();
        function2();
        function1();
    }
    Array<ScopeStatistics> stats = Profiler::getInstance()->getStatistics();
    REQUIRE(stats.size() == 3);
    REQUIRE(stats[0].name == "all");
    REQUIRE(stats[0].totalTime / 1000 == 170); // totalTime is in microseconds
    REQUIRE(stats[1].name == "function1");
    REQUIRE(stats[1].totalTime / 1000 == 100);
    REQUIRE(stats[2].name == "function2");
    REQUIRE(stats[2].totalTime / 1000 == 70);
}
