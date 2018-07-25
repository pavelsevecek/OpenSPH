#pragma once

/// \file Pool.h
/// \brief Simple thread pool with fixed number of threads
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/Array.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/Optional.h"
#include "thread/Scheduler.h"
#include <atomic>
#include <condition_variable>
#include <queue>
#include <thread>

NAMESPACE_SPH_BEGIN

/// \brief Task to be executed by one of available threads.
class Task : public ITask, public ShareFromThis<Task> {
private:
    std::condition_variable waitVar;
    std::mutex waitMutex;

    /// Number of child tasks + 1 for itself
    std::atomic<int> tasksLeft{ 1 };

    Function<void()> callable = nullptr;

    SharedPtr<Task> parent = nullptr;

    std::exception_ptr caughtException = nullptr;

public:
    explicit Task(const Function<void()>& callable);

    virtual void wait() override;

    virtual bool completed() const override;

    /// \brief Assigns a task that spawned this task.
    ///
    /// Can be nullptr if this is the root task.
    void setParent(SharedPtr<Task> parent);

    /// \brief Saves exception into the task.
    ///
    /// The exception propagates into the top-most task.
    void setException(std::exception_ptr exception);

    /// \brief Returns true if this is the top-most task.
    bool isRoot() const;

    SharedPtr<Task> getParent() const;

    /// \brief Returns the currently execute task, or nullptr if no task is currently executed on this thread.
    static SharedPtr<Task> getCurrent();

    void runAndNotify();

private:
    void addReference();

    void removeReference();
};

/// \brief Thread pool capable of executing tasks concurrently.
class ThreadPool : public IScheduler {
private:
    /// Threads managed by this pool
    Array<AutoPtr<std::thread>> threads;

    /// Queue of waiting tasks.
    std::queue<SharedPtr<Task>> tasks;

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

    /// Global instance of the ThreadPool.
    /// \note This is not a singleton, another instances can be created if needed.
    static SharedPtr<ThreadPool> globalInstance;

public:
    /// Initialize thread pool given the number of threads to use. By default, all available threads are used.
    ThreadPool(const Size numThreads = 0);

    ~ThreadPool();

    /// \brief Submits a task into the thread pool.
    ///
    /// The task will be executed asynchronously once tasks submitted before it are completed.
    virtual SharedPtr<ITask> submit(const Function<void()>& task) override;

    /// \brief Returns the index of this thread, or NOTHING if this thread was not invoked by the thread pool.
    ///
    /// The index is within [0, numThreads-1].
    virtual Optional<Size> getThreadIdx() const override;

    /// \brief Returns the number of threads used by this thread pool.
    ///
    /// Note that this number is constant during the lifetime of thread pool.
    virtual Size getThreadCnt() const override;

    virtual Size getRecommendedGranularity(const Size from, const Size to) const override;

    /// \brief Blocks until all submitted tasks has been finished.
    void waitForAll();

    /// \brief Returns the number of unfinished tasks.
    ///
    /// This includes both tasks currently running and tasks waiting in processing queue.
    Size remainingTaskCnt() {
        return tasksLeft;
    }

    /// \brief Returns the global instance of the thread pool.
    ///
    /// Other instances can be constructed if needed.
    static SharedPtr<ThreadPool> getGlobalInstance();

private:
    SharedPtr<Task> getNextTask();
};

NAMESPACE_SPH_END
