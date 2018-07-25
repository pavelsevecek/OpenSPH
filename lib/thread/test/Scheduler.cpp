#include "objects/utility/ArrayUtils.h"
#include "thread/Pool.h"
#include "thread/Tbb.h"
#include "thread/ThreadLocal.h"
#include "utils/Utils.h"

using namespace Sph;

TYPED_TEST_CASE_3("ThreadLocal", "[thread]", TScheduler, ThreadPool, Tbb, SequentialScheduler) {
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

TYPED_TEST_CASE_3("ThreadLocal parallelFor", "[thread]", TScheduler, ThreadPool, Tbb, SequentialScheduler) {
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
