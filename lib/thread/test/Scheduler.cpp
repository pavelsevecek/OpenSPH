#include "objects/utility/ArrayUtils.h"
#include "thread/Pool.h"
#include "thread/Tbb.h"
#include "thread/ThreadLocal.h"
#include "utils/Utils.h"

using namespace Sph;

TYPED_TEST_CASE_2("ThreadLocal", "[thread]", TScheduler, ThreadPool, Tbb) {
    TScheduler& scheduler = *TScheduler::getGlobalInstance();
    ThreadLocal<uint64_t> partialSum(scheduler);
    parallelFor(scheduler, 1, 100000, 10, [&partialSum](Size i) {
        uint64_t& value = partialSum.local();
        value += i;
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    });
    uint64_t sum = 0;
    const uint64_t expectedSum = 4999950000;
    const Size threadCnt = scheduler.getThreadCnt();
    const uint64_t sumPerThread = expectedSum / threadCnt;


    for (auto& value : partialSum) {

        // this can be very noisy, so lets be generous
        if (std::is_same<TScheduler, Tbb>::value) {
            // TBBs do not attempt to equalize the work in this way
            REQUIRE_THREAD_SAFE(value > 0);
        } else {
            REQUIRE_THREAD_SAFE(value >= sumPerThread / 2);
            REQUIRE_THREAD_SAFE(value <= sumPerThread * 2);
        }
        sum += value;
    }
    REQUIRE_THREAD_SAFE(sum == expectedSum);

    // note that occasionally the number of tasks can be still > 0, since rootTask is notified before CV in
    // thread pool.
    // REQUIRE_THREAD_SAFE(pool.remainingTaskCnt() == 0);
}

TYPED_TEST_CASE_2("ThreadLocal parallelFor", "[thread]", TScheduler, ThreadPool, Tbb) {
    TScheduler& scheduler = *TScheduler::getGlobalInstance();
    const Size N = 100000;
    ThreadLocal<Array<Size>> partial(scheduler, N);
    for (Array<Size>& value : partial) {
        value.fill(0);
    }

    std::atomic<int> executeCnt;
    executeCnt = 0;
    parallelFor(scheduler, partial, 0, N, 1, [&executeCnt](Size i, Array<Size>& value) {
        executeCnt++;
        value[i] = 1;
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    });
    REQUIRE_THREAD_SAFE(executeCnt == N);
    Array<Size> sum(N);
    sum.fill(0);
    for (Array<Size>& value : partial) {
        Size perThreadSum = 0;
        for (Size i = 0; i < sum.size(); ++i) {
            sum[i] += value[i];
            perThreadSum += value[i];
        }

        if (std::is_same<TScheduler, ThreadPool>::value) {
            // TBBs do not attempt to equalize the work in this way

            REQUIRE_THREAD_SAFE(perThreadSum > N / scheduler.getThreadCnt() - 2000);
            REQUIRE_THREAD_SAFE(perThreadSum < N / scheduler.getThreadCnt() + 2000);
        }
    }
    REQUIRE_THREAD_SAFE(areAllMatching(sum, [](const Size v) { return v == 1; }));
}

TEST_CASE("Concurrent parallelFor", "[thread]") {
    /// \todo the same for TBBs !!
    ThreadPool scheduler;
    std::atomic<uint64_t> sum1{ 0 };
    std::atomic<uint64_t> sum2{ 0 };
    auto for1 = scheduler.submit([&scheduler, &sum1] { //
        parallelFor(scheduler, 0, 10000, 10, [&sum1](Size i) { sum1 += i; });
    });
    auto for2 = scheduler.submit([&scheduler, &sum2] { //
        parallelFor(scheduler, 0, 10000, 10, [&sum2](Size i) { sum2 += i; });
    });
    for1->wait();
    for2->wait();
    const uint64_t expectedSum = 49995000;
    REQUIRE_THREAD_SAFE(sum1 == expectedSum);
    REQUIRE_THREAD_SAFE(sum2 == expectedSum);
}

TYPED_TEST_CASE_2("ThreadLocal accumulate", "[thread]", TScheduler, ThreadPool, Tbb) {
    TScheduler& scheduler = *TScheduler::getGlobalInstance();
    ThreadLocal<int64_t> sumTl(scheduler, 0._f);
    parallelFor(scheduler, sumTl, 0, 10000, 10, [](Size i, int64_t& value) { value += i; });
    const int64_t sum = sumTl.accumulate(12);
    const int64_t expectedSum = 49995012;
    REQUIRE_THREAD_SAFE(sum == expectedSum);

    const int64_t sum2 = sumTl.accumulate(25, [](int64_t i1, int64_t i2) { return i1 - i2; });
    const int64_t expectedSum2 = -49994975;
    REQUIRE_THREAD_SAFE(sum2 == expectedSum2);
}
