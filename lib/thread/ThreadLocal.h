#pragma once

/// \file ThreadLocal.h
/// \brief Template for thread-local storage
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/utility/Iterators.h"
#include "objects/wrappers/Optional.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

/// \brief Template for storing a copy of a value for every thread in given scheduler.
///
/// While C++ provides thread_local keyword for creating thread-local storages with static duration,
/// ThreadLocal template can be used for local variables or (non-static) member variables of classes.
template <typename Type>
class ThreadLocal {
    // befriend other ThreadLocal classes
    template <typename>
    friend class ThreadLocal;

private:
    struct Local {
        uint8_t padd1[64];
        Type value;
        uint8_t padd2[64];

        template <typename... TArgs>
        Local(TArgs&&... args)
            : value(std::forward<TArgs>(args)...) {}
    };

    /// Array of thread-local values
    Array<Local> locals;

    /// Associated scheduler; one value is allocated for each thread of the scheduler.
    IScheduler& scheduler;

public:
    /// \brief Constructs a thread-local storage.
    ///
    /// \param scheduler Scheduler associated with the object.
    /// \param args List of parameters that are passed into the constructor of each thread-local storage.
    template <typename... TArgs>
    ThreadLocal(IScheduler& scheduler, TArgs&&... args)
        : scheduler(scheduler) {
        const Size threadCnt = scheduler.getThreadCnt();
        locals.reserve(threadCnt);
        for (Size i = 0; i < threadCnt; ++i) {
            // intentionally not forwarded, we cannot move parameters if we have more than one object
            locals.emplaceBack(args...);
        }
    }

    /// \brief Return a value for current thread.
    ///
    /// This thread must belong the the thread pool given in constructor, checked by assert.
    INLINE Type& local() {
        const Optional<Size> idx = scheduler.getThreadIdx();
        ASSERT(idx && idx.value() < locals.size());
        return locals[idx.value()].value;
    }

    /// \copydoc local
    INLINE const Type& local() const {
        const Optional<Size> idx = scheduler.getThreadIdx();
        ASSERT(idx && idx.value() < locals.size());
        return locals[idx.value()].value;
    }

    /// \brief Creates another ThreadLocal object by converting each thread-local value of this object.
    ///
    /// The constructed object can have a different type than this object. The created thread-local values are
    /// default-constructed and assigned to using the given conversion functor.
    template <typename TOther, typename TFunctor>
    ThreadLocal<TOther> convert(TFunctor&& functor) {
        ThreadLocal<TOther> result(scheduler);
        for (Size i = 0; i < result.locals.size(); ++i) {
            result.locals[i].value = functor(locals[i].value);
        }
        return result;
    }

    /// \brief Creates another ThreadLocal object by converting each thread-local value of this object.
    ///
    /// This overload of the function uses explicit conversion defined by the type.
    template <typename TOther>
    ThreadLocal<TOther> convert() {
        return this->convert<TOther>([](Type& value) -> TOther { return value; });
    }

    /// \brief Returns the storage corresponding to the thread with given index.
    ///
    /// Can be called from any thread. There is no synchronization, so accessing the storage from the
    /// associated worker at the same time might cause a race condition.
    Type& value(const Size threadId) {
        return locals[threadId].value;
    }

    template <typename T>
    class LocalIterator : public Iterator<T> {
    public:
        LocalIterator(Iterator<T> iter)
            : Iterator<T>(iter) {}

        INLINE const Type& operator*() const {
            return this->data->value;
        }
        INLINE Type& operator*() {
            return this->data->value;
        }
    };

    /// \brief Returns the iterator to the first element in the thread-local storage.
    LocalIterator<Local> begin() {
        return locals.begin();
    }

    /// \copydoc begin
    LocalIterator<const Local> begin() const {
        return locals.begin();
    }

    /// \brief Returns the iterator to the first element in the thread-local storage.
    LocalIterator<Local> end() {
        return locals.end();
    }

    /// \copydoc end
    LocalIterator<const Local> end() const {
        return locals.end();
    }
};

/// \brief Overload of parallelFor that passes thread-local storage into the functor.
template <typename Type, typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler,
    ThreadLocal<Type>& storage,
    const Size from,
    const Size to,
    TFunctor&& functor) {
    const Size granularity = scheduler.getRecommendedGranularity(from, to);
    parallelFor(scheduler, storage, from, to, granularity, std::forward<TFunctor>(functor));
}

template <typename Type, typename TFunctor>
class ParallelForTlsTask : public Noncopyable {
private:
    Size from;
    Size to;
    const Size granularity;
    IScheduler& scheduler;
    ThreadLocal<Type>& storage;
    TFunctor& functor;

public:
    ParallelForTlsTask(Size from,
        Size to,
        Size granularity,
        IScheduler& scheduler,
        ThreadLocal<Type>& storage,
        TFunctor& functor)
        : from(from)
        , to(to)
        , granularity(granularity)
        , scheduler(scheduler)
        , storage(storage)
        , functor(functor) {}

    void operator()() {
        while (to - from > granularity) {
            const Size mid = (from + to) / 2;
            ASSERT(from < mid && mid < to);

            // split the task in half, submit the second half as a new task
            scheduler.submit(
                makeShared<ParallelForTlsTask>(mid, to, granularity, scheduler, storage, functor));

            // keep processing the first half in this task
            to = mid;
            ASSERT(from < to);
        }

        ASSERT(from < to);
        // when below the granularity, process sequentially
        Type& value = storage.local();
        for (Size n = from; n < to; ++n) {
            functor(n, value);
        }
    }
};

/// \brief Overload of parallelFor that passes thread-local storage into the functor.
template <typename Type, typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler,
    ThreadLocal<Type>& storage,
    const Size from,
    const Size to,
    const Size granularity,
    TFunctor&& functor) {
    ASSERT(from <= to);

    SharedPtr<ITask> handle = scheduler.submit(
        makeShared<ParallelForTlsTask<Type, TFunctor>>(from, to, granularity, scheduler, storage, functor));
    handle->wait();
}

NAMESPACE_SPH_END
