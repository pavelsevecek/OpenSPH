#pragma once

/// \file Pool.h
/// \brief Simple thread pool with fixed number of threads
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/Array.h"
#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/Optional.h"
#include "thread/IScheduler.h"
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>

NAMESPACE_SPH_BEGIN

/// \brief Thread pool capable of executing tasks concurrently
class ThreadPool : public IScheduler {
private:
    /// Threads managed by this pool
    Array<AutoPtr<std::thread>> threads;

    /// Queue of waiting tasks.
    std::queue<AutoPtr<ITask>> tasks;

    /// Used for synchronization of the task queue
    std::condition_variable taskVar;
    std::mutex taskMutex;

    /// Used for synchronization of task scheduling
    std::condition_variable waitVar;
    std::mutex waitMutex;

    /// Set to true if all tasks should be stopped ASAP
    std::atomic<bool> stop;

    /// Number of unprocessed tasks (either currently processing or waiting).
    std::atomic<int> tasksLeft;

    /// Last caught exception in one of worker threads.
    std::exception_ptr caughtException = nullptr;

    /// Global instance of the ThreadPool.
    /// \note This is not a singleton, another instances can be created if needed.
    static ThreadPool* globalInstance;

public:
    /// Initialize thread pool given the number of threads to use. By default, all available threads are used.
    ThreadPool(const Size numThreads = 0);

    ~ThreadPool();

    /// \brief Submits a task into the thread pool.
    ///
    /// The task will be executed asynchronously once tasks submitted before it are completed.
    virtual void submit(AutoPtr<ITask>&& task) override;

    /// Blocks until all submitted tasks has been finished.
    virtual void waitForAll() override;

    /// Returns the index of this thread, or NOTHING if this thread was not invoked by the thread pool.
    /// The index is within [0, numThreads-1].
    Optional<Size> getThreadIdx() const;

    /// \brief Returns the number of threads used by this thread pool.
    ///
    /// Note that this number is constant during the lifetime of thread pool.
    Size getThreadCnt() const {
        return threads.size();
    }

    /// \brief Returns the number of unfinished tasks.
    ///
    /// This includes both tasks currently running and tasks waiting in processing queue.
    Size remainingTaskCnt() {
        return tasksLeft;
    }

    /// Returns the global instance of the thread pool. Other instances can be constructed if needed.
    static ThreadPool& getGlobalInstance();

private:
    AutoPtr<ITask> getNextTask();
};

/// \brief Executes a functor concurrently, using an empirical formula for granularity.
///
/// This overload uses the global instance of the thread pool
template <typename TFunctor>
INLINE void parallelFor(const Size from, const Size to, TFunctor&& functor) {
    ThreadPool& pool = ThreadPool::getGlobalInstance();
    const Size granularity = min<Size>(1000, max<Size>((to - from) / pool.getThreadCnt(), 1));
    parallelFor(pool, from, to, granularity, std::forward<TFunctor>(functor));
}

NAMESPACE_SPH_END
