#include "thread/CheckFunction.h"
#include "catch.hpp"
#include "common/Assert.h"
#include "utils/Utils.h"
#include <thread>

using namespace Sph;

static void runOnce() {
    CHECK_FUNCTION(CheckFunction::ONCE);
}

static void reentrant() {
    CHECK_FUNCTION(CheckFunction::NON_REENRANT);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

static void mainThread() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
}

TEST_CASE("CheckFunction", "[checkfunction]") {
    REQUIRE_NOTHROW(runOnce());
    REQUIRE_ASSERT(runOnce());

    std::thread t([] { REQUIRE_NOTHROW(reentrant()); });
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    REQUIRE_ASSERT(reentrant());
    t.join();

    REQUIRE_NOTHROW(mainThread());
    t = std::thread([] { REQUIRE_ASSERT(mainThread()); });
}
