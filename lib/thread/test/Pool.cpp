#include "thread/Pool.h"
#include "catch.hpp"
#include "objects/utility/ArrayUtils.h"
#include "system/Timer.h"
#include "thread/ThreadLocal.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Submit task", "[thread]") {
    ThreadPool pool;
    REQUIRE(pool.getThreadCnt() == std::thread::hardware_concurrency());
    std::atomic<uint64_t> sum;
    sum = 0;
    for (Size i = 0; i <= 100; ++i) {
        pool.submit(makeTask([&sum, i] { sum += i; }));
    }
    pool.waitForAll();
    REQUIRE(sum == 5050);
    REQUIRE(pool.remainingTaskCnt() == 0);
}

TEST_CASE("Pool thread count", "[thread]") {
    ThreadPool pool1(0);
    REQUIRE(pool1.getThreadCnt() == std::thread::hardware_concurrency());
    ThreadPool pool2(5);
    REQUIRE(pool2.getThreadCnt() == 5);
}

TEST_CASE("Submit task from different thread", "[thread]") {
    ThreadPool pool;
    std::atomic<uint64_t> sum1, sum2;
    sum1 = sum2 = 0;
    std::thread thread([&sum2, &pool] {
        for (Size i = 0; i <= 100; i += 2) { // even numbers
            pool.submit(makeTask([&sum2, i] { sum2 += i; }));
        }
    });
    for (Size i = 1; i <= 100; i += 2) {
        pool.submit(makeTask([&sum1, i] { sum1 += i; }));
    }
    thread.join();
    pool.waitForAll();
    REQUIRE(sum1 + sum2 == 5050);
    REQUIRE(pool.remainingTaskCnt() == 0);
}

TEST_CASE("Submit single", "[thread]") {
    ThreadPool pool;
    bool executed = false;
    pool.submit(makeTask([&executed] { executed = true; }));
    pool.waitForAll();
    REQUIRE(pool.remainingTaskCnt() == 0);
    REQUIRE(executed);
}

TEST_CASE("ParallelFor", "[thread]") {
    ThreadPool pool;
    std::atomic<uint64_t> sum;
    sum = 0;
    parallelFor(pool, 1, 100000, [&sum](Size i) { sum += i; });
    REQUIRE(sum == 4999950000);
    REQUIRE(pool.remainingTaskCnt() == 0);
}

TEST_CASE("GetThreadIdx", "[thread]") {
    ThreadPool pool(2);
    REQUIRE(pool.getThreadCnt() == 2);
    REQUIRE_FALSE(pool.getThreadIdx()); // main thread, not within the pool

    std::thread thread([&pool] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        REQUIRE_FALSE(pool.getThreadIdx()); // also not within the pool
    });
    thread.join();

    pool.submit(makeTask([&pool] {
        const Optional<Size> idx = pool.getThreadIdx();
        REQUIRE(idx);
        REQUIRE((idx.value() == 0 || idx.value() == 1));
    }));
}

TEST_CASE("WaitForAll", "[thread]") {
    ThreadPool pool;
    pool.waitForAll(); // waitForAll with no running tasks

    Timer timer;
    const Size cnt = pool.getThreadCnt();
    std::atomic_int taskIdx;
    taskIdx = 0;
    // run tasks with different duration
    for (Size i = 0; i < cnt; ++i) {
        pool.submit(
            makeTask([&taskIdx] { std::this_thread::sleep_for(std::chrono::milliseconds(50 * ++taskIdx)); }));
    }
    pool.waitForAll();
    REQUIRE(timer.elapsed(TimerUnit::MILLISECOND) >= 50 * cnt);
    REQUIRE_NOTHROW(pool.waitForAll()); // second does nothing
}

TEST_CASE("ThreadLocal", "[thread]") {
    ThreadPool pool;
    ThreadLocal<uint64_t> partialSum(pool);
    parallelFor(pool, 1, 100000, [&partialSum](Size i) {
        uint64_t& value = partialSum.get();
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
    REQUIRE(pool.remainingTaskCnt() == 0);
}

TEST_CASE("ThreadLocal parallelFor", "[thread]") {
    ThreadPool pool;
    const Size N = 100000;
    ThreadLocal<Array<Size>> partial(pool, N);
    partial.forEach([](Array<Size>& value) { value.fill(0); });

    std::atomic<int> executeCnt;
    executeCnt = 0;
    parallelFor(pool, partial, 0, N, 1, [&executeCnt](Size i, Array<Size>& value) {
        executeCnt++;
        value[i] = 1;
    });
    REQUIRE(executeCnt == N);
    Array<Size> sum(N);
    sum.fill(0);
    partial.forEach([&](Array<Size>& value) {
        Size perThreadSum = 0;
        for (Size i = 0; i < sum.size(); ++i) {
            sum[i] += value[i];
            perThreadSum += value[i];
        }
        REQUIRE(perThreadSum > N / pool.getThreadCnt() - 2000);
        REQUIRE(perThreadSum < N / pool.getThreadCnt() + 2000);
    });
    REQUIRE(areAllMatching(sum, [](const Size v) { return v == 1; }));
    REQUIRE(pool.remainingTaskCnt() == 0);
}

#ifdef SPH_DEBUG
TEST_CASE("ParallelFor throw", "[thread]") {
    ThreadPool pool(2);
    // throw from worker thread
    auto lambda = [](Size) { throw Assert::Exception("exception1"); };

    REQUIRE_ASSERT(parallelFor(pool, 1, 2, lambda));
}
#endif
