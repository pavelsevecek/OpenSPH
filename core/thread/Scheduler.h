#pragma once

/// \file Scheduler.h
/// \brief Interface for executing tasks (potentially) asynchronously.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/ForwardDecl.h"
#include "objects/utility/IteratorAdapters.h"
#include "objects/wrappers/Function.h"

NAMESPACE_SPH_BEGIN

/// \brief Interface that allows unified implementation of sequential and parallelized versions of algorithms.
///
/// Currently suitable only for task-based schedulers, cannot be used for OpenMP, MPI, etc.
class IScheduler : public Polymorphic {
public:
    /// \brief Returns the index of the calling thread.
    ///
    /// If this thread was not invoked by the scheduler, returns NOTHING. The returned index is interval
    /// [0, getThreadCnt()-1].
    virtual Optional<Size> getThreadIdx() const = 0;

    /// \brief Returns the number of threads used by this scheduler.
    ///
    /// Note that this number is constant during the lifetime of the scheduler.
    virtual Size getThreadCnt() const = 0;

    /// \brief Returns a value of granularity that is expected to perform well with the current thread count.
    virtual Size getRecommendedGranularity() const = 0;

    using Functor = Function<void()>;
    using RangeFunctor = Function<void(Size n1, Size n2)>;

    /// \brief Processes the given range concurrently.
    ///
    /// \param from First index of the processed range.
    /// \param to One-past-last index of the processed range.
    /// \param granularity Recommended size of the chunks passed to the functor.
    /// \param functor Functor executed concurrently by the worker threads. Takes the first and the
    ///                one-past-last index of the chunk to process sequentially within the thread.
    virtual void parallelFor(const Size from,
        const Size to,
        const Size granularity,
        const RangeFunctor& functor) = 0;

    /// \brief Executes two functors concurrently.
    virtual void parallelInvoke(const Functor& task1, const Functor& task2) = 0;
};

/// \brief Dummy scheduler that simply executes the submitted tasks sequentially on calling thread.
///
/// Useful to run an algorithm with no parallelization, mainly for testing/debugging purposes.
class SequentialScheduler : public IScheduler {
public:
    virtual Optional<Size> getThreadIdx() const override;

    virtual Size getThreadCnt() const override;

    virtual Size getRecommendedGranularity() const override;

    virtual void parallelFor(const Size from,
        const Size to,
        const Size granularity,
        const RangeFunctor& functor) override;

    virtual void parallelInvoke(const Functor& func1, const Functor& func2) override;

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
    const Size granularity = scheduler.getRecommendedGranularity();
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
        SPH_ASSERT(n1 <= n2);
        for (Size i = n1; i < n2; ++i) {
            functor(i);
        }
    });
}

/// \brief Executes a functor concurrently from all available threads.
///
/// Overload using an index sequence.
template <typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler, const IndexSequence& sequence, TFunctor&& functor) {
    parallelFor(scheduler, *sequence.begin(), *sequence.end(), std::forward<TFunctor>(functor));
}

/// \brief Syntactic sugar, calls \ref parallelInvoke in given scheduler.
template <typename TFunctor1, typename TFunctor2>
void parallelInvoke(IScheduler& scheduler, TFunctor1&& func1, TFunctor2&& func2) {
    scheduler.parallelInvoke(std::forward<TFunctor1>(func1), std::forward<TFunctor2>(func2));
}

NAMESPACE_SPH_END
