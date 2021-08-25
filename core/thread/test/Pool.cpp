#include "thread/Pool.h"
#include "catch.hpp"
#include "system/Timer.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Pool submit task", "[thread]") {
    ThreadPool pool;
    REQUIRE_THREAD_SAFE(pool.getThreadCnt() == std::thread::hardware_concurrency());
    std::atomic<uint64_t> sum;
    sum = 0;
    for (Size i = 0; i <= 100; ++i) {
        pool.submit([&sum, i] { sum += i; });
    }
    pool.waitForAll();
    REQUIRE_THREAD_SAFE(sum == 5050);
    REQUIRE_THREAD_SAFE(pool.remainingTaskCnt() == 0);
}

TEST_CASE("Pool thread count", "[thread]") {
    ThreadPool pool1(0);
    REQUIRE_THREAD_SAFE(pool1.getThreadCnt() == std::thread::hardware_concurrency());
    ThreadPool pool2(5);
    REQUIRE_THREAD_SAFE(pool2.getThreadCnt() == 5);
}

TEST_CASE("Pool submit task from different thread", "[thread]") {
    ThreadPool pool;
    std::atomic<uint64_t> sum1, sum2;
    sum1 = sum2 = 0;
    std::thread thread([&sum2, &pool] {
        for (Size i = 0; i <= 100; i += 2) { // even numbers
            pool.submit([&sum2, i] { sum2 += i; });
        }
    });
    for (Size i = 1; i <= 100; i += 2) {
        pool.submit([&sum1, i] { sum1 += i; });
    }
    thread.join();
    pool.waitForAll();
    REQUIRE_THREAD_SAFE(sum1 + sum2 == 5050);
    REQUIRE_THREAD_SAFE(pool.remainingTaskCnt() == 0);
}

TEST_CASE("Pool submit single", "[thread]") {
    ThreadPool pool;
    bool executed = false;
    pool.submit([&executed] { executed = true; });
    pool.waitForAll();
    REQUIRE_THREAD_SAFE(pool.remainingTaskCnt() == 0);
    REQUIRE_THREAD_SAFE(executed);
}

TEST_CASE("Pool one thread", "[thread]") {
    ThreadPool pool(1);
    int executed = 0;
    auto task = [&executed] {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        ++executed;
    };
    for (Size i = 0; i < 4; ++i) {
        pool.submit(task);
    }
    pool.waitForAll();
    REQUIRE(executed == 4);
}

TEST_CASE("Pool submit nested", "[thread]") {
    ThreadPool pool;
    std::atomic_bool innerRun{ 0 };
    auto rootTask = pool.submit([&pool, &innerRun] {
        REQUIRE_THREAD_SAFE(Task::getCurrent());
        REQUIRE_THREAD_SAFE(Task::getCurrent()->isRoot());

        WeakPtr<Task> parent = Task::getCurrent();
        pool.submit([parent, &innerRun] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

            auto task = Task::getCurrent();
            REQUIRE_THREAD_SAFE(task);
            REQUIRE_THREAD_SAFE(!task->isRoot());
            REQUIRE_THREAD_SAFE(task->getParent() == parent.lock());
            innerRun = true;
        });
    });
    REQUIRE_THREAD_SAFE(!rootTask->completed());
    REQUIRE_THREAD_SAFE(!innerRun);
    rootTask->wait();
    REQUIRE_THREAD_SAFE(innerRun);
    REQUIRE_THREAD_SAFE(rootTask->completed());
    REQUIRE_THREAD_SAFE(pool.remainingTaskCnt() == 0);

    // second doesn't do anything
    rootTask->wait();

    // pool.waitForAll();
}

TEST_CASE("Pool submit parallel", "[thread]") {
    // checks that we can wait for two tasks to finish independently
    ThreadPool pool;
    auto task1 = pool.submit([] {
        REQUIRE_THREAD_SAFE(Task::getCurrent());
        REQUIRE_THREAD_SAFE(Task::getCurrent()->isRoot());
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    });

    auto task2 = pool.submit([] {
        REQUIRE_THREAD_SAFE(Task::getCurrent());
        REQUIRE_THREAD_SAFE(Task::getCurrent()->isRoot());
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
    });

    REQUIRE_THREAD_SAFE(!task1->completed());
    REQUIRE_THREAD_SAFE(!task2->completed());
    task1->wait();
    REQUIRE_THREAD_SAFE(task1->completed());
    REQUIRE_THREAD_SAFE(!task2->completed());
    task2->wait();
    REQUIRE_THREAD_SAFE(task2->completed());
    REQUIRE_THREAD_SAFE(pool.remainingTaskCnt() == 0);

    // now the same thing, but wait for the second (longer) one
    task1 = pool.submit([] { std::this_thread::sleep_for(std::chrono::milliseconds(20)); });
    task2 = pool.submit([] { std::this_thread::sleep_for(std::chrono::milliseconds(60)); });
    REQUIRE_THREAD_SAFE(!task1->completed());
    REQUIRE_THREAD_SAFE(!task2->completed());
    task2->wait();
    REQUIRE_THREAD_SAFE(task1->completed());
    REQUIRE_THREAD_SAFE(task2->completed());

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    REQUIRE_THREAD_SAFE(pool.remainingTaskCnt() == 0);

    // pool.waitForAll();
}

TEST_CASE("Pool wait for child", "[thread]") {
    ThreadPool pool;
    SharedPtr<Task> taskRoot, taskChild;
    volatile bool childFinished = false;
    taskRoot = pool.submit([&pool, &taskChild, &childFinished] {
        taskChild = pool.submit([&childFinished] {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            childFinished = true;
        });
        taskChild->wait();
    });
    taskRoot->wait();

    REQUIRE_THREAD_SAFE(taskRoot->completed());
    REQUIRE_THREAD_SAFE(taskChild->completed());
    REQUIRE_THREAD_SAFE(childFinished);
}

class TestException : public std::exception {
    virtual const char* what() const noexcept override {
        return "exception";
    }
};

TEST_CASE("Pool task throw", "[thread]") {
    ThreadPool pool;

    auto task = pool.submit([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        throw TestException();
    });
    REQUIRE_THROWS_AS(task->wait(), TestException);
}

TEST_CASE("Pool task throw nested", "[thread]") {
    ThreadPool pool;

    auto task = pool.submit([&pool] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        pool.submit([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            throw TestException();
        });
    });
    REQUIRE_THROWS_AS(task->wait(), TestException);
}

TEST_CASE("Pool ParallelFor", "[thread]") {
    ThreadPool pool;
    std::atomic<uint64_t> sum;
    sum = 0;
    parallelFor(pool, 1, 100000, [&sum](Size i) { sum += i; });
    REQUIRE_THREAD_SAFE(sum == 4999950000);
    REQUIRE_THREAD_SAFE(pool.remainingTaskCnt() == 0);
}

TEST_CASE("Pool GetThreadIdx", "[thread]") {
    ThreadPool pool(2);
    REQUIRE_THREAD_SAFE(pool.getThreadCnt() == 2);
    REQUIRE_FALSE(pool.getThreadIdx()); // main thread, not within the pool

    std::thread thread([&pool] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        REQUIRE_THREAD_SAFE(!pool.getThreadIdx()); // also not within the pool
    });
    thread.join();

    pool.submit([&pool] {
        const Optional<Size> idx = pool.getThreadIdx();
        REQUIRE_THREAD_SAFE(idx);
        REQUIRE_THREAD_SAFE((idx.value() == 0 || idx.value() == 1));
    });
}

TEST_CASE("Pool WaitForAll", "[thread]") {
    ThreadPool pool;
    pool.waitForAll(); // waitForAll with no running tasks

    Timer timer;
    const Size cnt = pool.getThreadCnt();
    std::atomic_int taskIdx;
    taskIdx = 0;
    // run tasks with different duration
    for (Size i = 0; i < cnt; ++i) {
        pool.submit([&taskIdx] { std::this_thread::sleep_for(std::chrono::milliseconds(50 * ++taskIdx)); });
    }
    pool.waitForAll();
    REQUIRE_THREAD_SAFE(timer.elapsed(TimerUnit::MILLISECOND) >= 50 * cnt);
    REQUIRE_NOTHROW(pool.waitForAll()); // second does nothing
}

#ifdef SPH_DEBUG
TEST_CASE("Pool ParallelFor assert", "[thread]") {
    ThreadPool pool(2);
    // throw from worker thread
    auto lambda = [](Size) { SPH_ASSERT(false); };

    REQUIRE_SPH_ASSERT(parallelFor(pool, 1, 2, lambda));
}
#endif
