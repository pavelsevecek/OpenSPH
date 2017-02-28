#pragma once

/// Thread pool
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/Array.h"
#include "thread/ConcurrentQueue.h"
#include <atomic>
#include <condition_variable>
#include <thread>

NAMESPACE_SPH_BEGIN

class ThreadPool : public Noncopyable {
private:
    Array<std::unique_ptr<std::thread>> threads;

    using Task = std::function<void(void)>;
    ConcurrentQueue<Task> tasks;

    std::condition_variable needTask;
    std::condition_variable emptyQueue;
    std::mutex mutex;

    std::atomic<bool> stop;

public:
    /// Initialize thread pool given the number of threads to use. By default, all available threads are used.
    ThreadPool(const Size numThreads = std::thread::hardware_concurrency())
        : threads(numThreads) {
        // start all threads
        auto loop = [this]() {
            Optional<Task> task = NOTHING;
            while (!stop) {
                if (task) {
                    task.get()();
                    task = tasks.pop();
                } else {
                    std::unique_lock<std::mutex> lock(mutex);
                    // notify that we have no tasks to process
                    emptyQueue.notify_one();
                    // wait till another task is submitted
                    needTask.wait(lock, [this, &task] { return stop || (task = tasks.pop()); });
                }
            }
        };
        stop = false;
        for (Size i = 0; i < numThreads; ++i) {
            threads[i] = std::make_unique<std::thread>(loop);
        }
    }

    ~ThreadPool() {
        waitForAll();
        std::unique_lock<std::mutex> lock(mutex);
        stop = true;
        lock.unlock();
        needTask.notify_all();
        for (Size i = 0; i < threads.size(); ++i) {
            threads[i]->join();
        }
        ASSERT(tasks.empty());
    }

    /// Submits a task into the thread pool. The task will be executed asynchronously once tasks submitted
    /// before it are completed.
    template <typename TFunctor>
    void submit(TFunctor&& functor) {
        std::unique_lock<std::mutex> lock(mutex);
        tasks.push(std::forward<TFunctor>(functor));
        needTask.notify_one();
    }

    /// Blocks until all submitted tasks has been finished.
    void waitForAll() {
        std::unique_lock<std::mutex> lock(mutex);
        if (!tasks.empty()) {
            emptyQueue.wait(lock, [this] { return tasks.empty(); });
        }
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
