#pragma once

/// \file ThreadLocal.h
/// \brief Template for thread-local storage
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/AlignedStorage.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

/// \brief Template for storing a copy of a value for every thread in given thread pool.
///
/// While C++ provides thread_local keyword for creating thread-local storages with static duration,
/// ThreadLocal template can be used for local variables or (non-static) member variables of classes.
template <typename Type>
class ThreadLocal {
    // befriend other ThreadLocal classes
    template <typename>
    friend class ThreadLocal;

private:
    /// Array of thread-local values
    Array<Type> values;

    /// Thread pool, threads of which correspond to the values
    ThreadPool& pool;

public:
    /// Constructs a thread-local storage.
    /// \param pool Thread pool associated with the object.
    /// \param args List of parameters that are passed into the constructor of each thread-local storage.
    template <typename... TArgs>
    ThreadLocal(ThreadPool& pool, TArgs&&... args)
        : pool(pool) {
        const Size threadCnt = pool.getThreadCnt();
        values.reserve(threadCnt);
        for (Size i = 0; i < threadCnt; ++i) {
            // intentionally not forwarded, we cannot move parameters if we have more than one object
            values.emplaceBack(args...);
        }
    }

    /// \brief Return a value for current thread.
    ///
    /// This thread must belong the the thread pool given in constructor, checked by assert.
    INLINE Type& get() {
        const Optional<Size> idx = pool.getThreadIdx();
        ASSERT(idx && idx.value() < values.size());
        return values[idx.value()];
    }

    /// \copydoc get
    INLINE const Type& get() const {
        const Optional<Size> idx = pool.getThreadIdx();
        ASSERT(idx && idx.value() < values.size());
        return values[idx.value()];
    }

    /// \brief Enumerate all thread-local storages and pass them into the argument of given functor.
    template <typename TFunctor>
    void forEach(TFunctor&& functor) {
        for (auto& value : values) {
            functor(value);
        }
    }

    /// \copydoc forEach
    template <typename TFunctor>
    void forEach(TFunctor&& functor) const {
        for (const auto& value : values) {
            functor(value);
        }
    }

    /// \brief Creates another ThreadLocal object by converting each thread-local value of this object.
    ///
    /// The constructed object can have a different type than this object. The created thread-local values are
    /// default-constructed and assigned to using the given conversion functor.
    template <typename TOther, typename TFunctor>
    ThreadLocal<TOther> convert(TFunctor&& functor) {
        ThreadLocal<TOther> result(pool);
        for (Size i = 0; i < result.values.size(); ++i) {
            result.values[i] = functor(values[i]);
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

    /// \brief Returns the storage corresponding to the first thread in the thread pool.
    ///
    /// Can be called from any thread. There is no synchronization, so accessing the storage from the
    /// associated worker at the same time might cause a race condition. Useful to share data between
    /// parallelized and single-threaded code without creating auxiliary storage for single-threaded
    /// usage.
    Type& first() {
        return values[0];
    }
};

/// Overload of parallelFor that passes thread-local storage into the functor. Function split range into
/// sub-ranges and passes beginning and end of each subrange to functors.
template <typename Type, typename TFunctor>
INLINE void parallelFor(ThreadPool& pool,
    ThreadLocal<Type>& storage,
    const Size from,
    const Size to,
    const Size granularity,
    TFunctor&& functor) {
    ASSERT(from <= to);
    for (Size i = from; i < to; i += granularity) {
        const Size n1 = i;
        const Size n2 = min(i + granularity, to);
        pool.submit(makeTask([n1, n2, &storage, &functor] {
            Type& tls = storage.get();
            functor(n1, n2, tls);
        }));
    }
    pool.waitForAll();
}

NAMESPACE_SPH_END
