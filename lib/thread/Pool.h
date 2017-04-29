#pragma once

/// Thread pool
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "thread/ConcurrentQueue.h"
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>

NAMESPACE_SPH_BEGIN

class ThreadPool : public Noncopyable {
private:
    Array<std::unique_ptr<std::thread>> threads;

    using Task = std::function<void(void)>;
    std::queue<Task> tasks;

    std::condition_variable taskVar;
    std::mutex taskMutex;
    std::condition_variable waitVar;
    std::mutex waitMutex;

    std::atomic<bool> stop;
    std::atomic<int> tasksLeft;

public:
    /// Initialize thread pool given the number of threads to use. By default, all available threads are used.
    ThreadPool(const Size numThreads = 0)
        : threads(numThreads == 0 ? std::thread::hardware_concurrency() : numThreads) {
        ASSERT(!threads.empty());
        auto loop = [this] {
            while (!stop) {
                Optional<Task> task = getNextTask();
                if (task) {
                    task.value()();
                    std::unique_lock<std::mutex> lock(waitMutex);
                    --tasksLeft;
                }
                waitVar.notify_one();
            }
        };
        stop = false;
        tasksLeft = 0;
        for (auto& t : threads) {
            t = std::make_unique<std::thread>(loop);
        }
    }

    ~ThreadPool() {
        waitForAll();
        stop = true;
        taskVar.notify_all();

        for (auto& t : threads) {
            if (t->joinable()) {
                t->join();
            }
        }
    }

    /// Submits a task into the thread pool. The task will be executed asynchronously once tasks submitted
    /// before it are completed.
    template <typename TFunctor>
    void submit(TFunctor&& functor) {
        {
            std::unique_lock<std::mutex> lock(taskMutex);
            tasks.emplace(std::forward<TFunctor>(functor));
        }
        {
            std::unique_lock<std::mutex> lock(waitMutex);
            ++tasksLeft;
        }
        taskVar.notify_one();
    }

    /// Blocks until all submitted tasks has been finished.
    void waitForAll() {
        std::unique_lock<std::mutex> lock(waitMutex);
        if (tasksLeft > 0) {
            waitVar.wait(lock, [this] { return tasksLeft == 0; });
        }
        ASSERT(tasks.empty() && tasksLeft == 0);
    }

    /// Returns the index of this thread, or NOTHING if this thread was not invoked by the thread pool.
    /// The index is within [0, numThreads-1].
    Optional<Size> getThreadIdx() const {
        std::thread::id id = std::this_thread::get_id();
        for (Size i = 0; i < threads.size(); ++i) {
            if (threads[i]->get_id() == id) {
                return i;
            }
        }
        return NOTHING;
    }

    /// Returns the number of threads used by this thread pool. Note that this number is constant during the
    /// lifetime of thread pool.
    Size getThreadCnt() const {
        return threads.size();
    }

    /// Returns the number of unfinished tasks, including both tasks currently running and tasks waiting in
    /// processing queue.
    Size remainingTaskCnt() {
        return tasksLeft;
    }

private:
    Optional<Task> getNextTask() {
        std::unique_lock<std::mutex> lock(taskMutex);

        // wait till a task is available
        taskVar.wait(lock, [this] { return tasks.size() || stop; });
        // execute task
        if (!stop && !tasks.empty()) {
            Task task = tasks.front();
            tasks.pop();
            return task;
        } else {
            return NOTHING;
        }
    }
};


/// Executes a functor concurrently. Syntax mimics typical usage of for loop; functor is executed with index
/// as parameter, starting at 'from' and ending one before 'to', so that total number of executions is
/// (to-from). The function blocks until parallel for is completed.
template <typename TFunctor>
INLINE void parallelFor(ThreadPool& pool, const Size from, const Size to, TFunctor&& functor) {
    for (Size i = from; i < to; ++i) {
        pool.submit([i, &functor] { functor(i); });
    }
    pool.waitForAll();
}

NAMESPACE_SPH_END
