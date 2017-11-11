#pragma once

#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

/// \brief Task to be executed by one of available threads
class ITask : public Polymorphic {
public:
    /// Executes the task.
    virtual void operator()() = 0;
};

/// \todo
class IScheduler : public Polymorphic {
public:
    /// \brief Submits a task into the thread pool.
    ///
    /// The task will be executed asynchronously once tasks submitted before it are completed.
    virtual void submit(AutoPtr<ITask>&& task) = 0;

    /// Blocks until all submitted tasks has been finished.
    virtual void waitForAll() = 0;
};

/// \brief Executes a functor concurrently from all available threads.
///
/// Syntax mimics typical usage of for loop; functor is executed with index as parameter, starting at 'from'
/// and ending one before 'to', so that total number of executions is (to-from). The function blocks until
/// parallel for is completed.
/// \param pool Thread pool, the functor will be executed on threads managed by this pool.
/// \param from First processed index.
/// \param to One-past-last processed index.
/// \param functor Functor executed (to-from) times in different threads; takes an index as an argument.
template <typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler, const Size from, const Size to, TFunctor&& functor) {
    for (Size i = from; i < to; ++i) {
        scheduler.submit(makeTask([i, &functor] { functor(i); }));
    }
    scheduler.waitForAll();
}

/// \brief Executes a functor concurrently with given granularity.
///
/// \param pool Thread pool, the functor will be executed on threads managed by this pool.
/// \param from First processed index.
/// \param to One-past-last processed index.
/// \param granularity Number of indices processed by the functor at once. It shall be a positive number less
///                    than or equal to (to-from).
/// \param functor Functor executed concurrently, takes two parameters as arguments, defining range of
///                assigned indices.
template <typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler,
    const Size from,
    const Size to,
    const Size granularity,
    TFunctor&& functor) {
    for (Size i = from; i < to; i += granularity) {
        const Size n1 = i;
        const Size n2 = min(i + granularity, to);
        scheduler.submit(makeTask([n1, n2, &functor] { functor(n1, n2); }));
    }
    scheduler.waitForAll();
}

NAMESPACE_SPH_END
