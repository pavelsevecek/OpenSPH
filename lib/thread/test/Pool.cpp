#include "thread/Pool.h"
#include "catch.hpp"
#include "thread/ThreadLocal.h"

using namespace Sph;


TEST_CASE("ParallelFor", "[thread]") {
    ThreadPool pool;
    std::atomic<uint64_t> sum;
    sum = 0;
    parallelFor(pool, 1, 100000, [&sum](Size i) { sum += i; });
    REQUIRE(sum == 4999950000);
}

TEST_CASE("GetThreadIdx", "[thread]") {
    ThreadPool pool(2);
    REQUIRE(pool.getThreadCnt() == 2);
    REQUIRE_FALSE(pool.getThreadIdx()); // main thread, not within the pool

    pool.submit([&pool] {
        const Optional<Size> idx = pool.getThreadIdx();
        REQUIRE(idx);
        REQUIRE((idx.get() == 0 || idx.get() == 1));
    });
}

TEST_CASE("ThreadLocal", "[thread]") {
    ThreadPool pool;
    ThreadLocal<std::atomic<uint64_t>> partialSum(pool);
    parallelFor(pool, 1, 100000, [&partialSum](Size i) {
        std::atomic<uint64_t>& value = partialSum.get();
        value += i;
    });
    uint64_t sum = 0;
    const uint64_t expectedSum = 4999950000;
    const Size threadCnt = pool.getThreadCnt();
    const uint64_t sumPerThread = expectedSum / threadCnt;

    partialSum.forEach([&sum, sumPerThread](auto&& value) {
        // this can be very noisy, so lets be generous
        REQUIRE(value >= sumPerThread / 2);
        REQUIRE(value <= sumPerThread * 2);
        sum += value;
    });
    REQUIRE(sum == expectedSum);
}
