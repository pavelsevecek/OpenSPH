#pragma once

/// \file Scheduler.h
/// \brief Interface for executing tasks (potentially) asynchronously.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/ForwardDecl.h"
#include "objects/wrappers/Function.h"

NAMESPACE_SPH_BEGIN

/// \brief Handle used to control tasks submitted into the scheduler.
class ITask : public Polymorphic {
public:
    /// \brief Waits till the task and all the child tasks are completed.
    virtual void wait() = 0;

    /// \brief Checks if the task already finished.
    virtual bool completed() const = 0;
};

/// \brief Interface that allows unified implementation of sequential and parallelized versions of algorithms.
///
/// Currently suitable only for task-based schedulers, cannot be used for OpenMP, MPI, etc.
class IScheduler : public Polymorphic {
public:
    /// \brief Submits a task to be potentially executed asynchronously.
    ///
    /// \return Handle to the task created from the functor.
    virtual SharedPtr<ITask> submit(const Function<void()>& task) = 0;

    /// \brief Returns the index of the calling thread.
    ///
    /// If this thread was not invoked by the scheduler, returns NOTHING. The returned index is interval
    /// [0, getThreadCnt()-1].
    virtual Optional<Size> getThreadIdx() const = 0;

    /// \brief Returns the number of threads used by this scheduler.
    ///
    /// Note that this number is constant during the lifetime of the scheduler.
    virtual Size getThreadCnt() const = 0;

    /// \brief Returns a value of granularity for (to - from) tasks that is expected to perform well with
    /// the current thread count.
    virtual Size getRecommendedGranularity(const Size from, const Size to) const = 0;

    /// \brief Processes the given range concurrently.
    ///
    /// Default implementation simply divides the range into chunks with size specified by granularity and
    /// submits them into the scheduler. It can be overriden by the derived class to provide an optimized
    /// variant of the parallel for.
    /// \param from First index of the processed range.
    /// \param to One-past-last index of the processed range.
    /// \param granularity Recommended size of the chunks passed to the functor.
    /// \param functor Functor executed concurrently by the worker threads. Takes the first and the
    ///                one-past-last index of the chunk to process sequentially within the thread.
    virtual void parallelFor(const Size from,
        const Size to,
        const Size granularity,
        const Function<void(Size n1, Size n2)>& functor);
};

/// \brief Dummy scheduler that simply executes the submitted tasks sequentially on calling thread.
///
/// Useful to run an algorithm with no parallelization, mainly for testing/debugging purposes.
class SequentialScheduler : public IScheduler {
public:
    virtual SharedPtr<ITask> submit(const Function<void()>& task) override;

    virtual Optional<Size> getThreadIdx() const override;

    virtual Size getThreadCnt() const override;

    virtual Size getRecommendedGranularity(const Size from, const Size to) const override;

    static SharedPtr<SequentialScheduler> getGlobalInstance();
};

/// \brief Global instance of the sequential scheduler.
///
/// It can be used to specify sequential execution policy for parallel algorithms, without creating
/// unnecessary local instances of \ref SequentialScheduler.
extern SequentialScheduler SEQUENTIAL;


/// \brief Executes a functor concurrently from all available threads.
///
/// Syntax mimics typical usage of for loop; functor is executed with index as parameter, starting at 'from'
/// and ending one before 'to', so that total number of executions is (to-from). The function blocks until
/// parallel for is completed.
/// \param scheduler Scheduler used for parallelization. If SequentialScheduler, this is essentailly ordinary
///                  for loop (with bigger overhead).
/// \param from First processed index.
/// \param to One-past-last processed index.
/// \param functor Functor executed (to-from) times in different threads; takes an index as an argument.
template <typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler, const Size from, const Size to, TFunctor&& functor) {
    const Size granularity = scheduler.getRecommendedGranularity(from, to);
    parallelFor(scheduler, from, to, granularity, std::forward<TFunctor>(functor));
}

/// \brief Executes a functor concurrently with given granularity.
///
/// Overload allowing to specify the granularity.
/// \param scheduler Scheduler used for parallelization.
/// \param from First processed index.
/// \param to One-past-last processed index.
/// \param granularity Number of indices processed by the functor at once. It shall be a positive number less
///                    than or equal to (to-from).
/// \param functor Functor executed concurrently, takes an index as an argument.
template <typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler,
    const Size from,
    const Size to,
    const Size granularity,
    TFunctor&& functor) {
    scheduler.parallelFor(from, to, granularity, [&functor](Size n1, Size n2) {
        ASSERT(n1 < n2);
        for (Size i = n1; i < n2; ++i) {
            functor(i);
        }
    });
}


NAMESPACE_SPH_END