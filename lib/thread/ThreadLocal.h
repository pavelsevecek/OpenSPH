#pragma once

/// \file ThreadLocal.h
/// \brief Template for thread-local storage
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/AlignedStorage.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

template <typename Type>
class ThreadLocal {
private:
    Array<AlignedStorage<Type>> values;
    ThreadPool& pool;

public:
    /// Constructs a thread-local storage, given thread pool and a list of parameters that are passed into the
    /// constructor of each storage.
    template <typename... TArgs>
    ThreadLocal(ThreadPool& pool, TArgs&&... args)
        : pool(pool) {
        values.resize(pool.getThreadCnt());
        for (auto& value : values) {
            // intentionally not forwarded, we cannot move parameters if we have more than one object
            value.emplace(args...);
        }
    }

    ~ThreadLocal() {
        for (auto& value : values) {
            value.destroy();
        }
    }

    /// Return a value for current thread. This thread must belong the the thread pool given in constructor,
    /// checked by assert.
    Type& get() {
        const Optional<Size> idx = pool.getThreadIdx();
        ASSERT(idx && idx.value() < values.size());
        return values[idx.value()].get();
    }

    /// Return a value for current thread, const version.
    const Type& get() const {
        const Optional<Size> idx = pool.getThreadIdx();
        ASSERT(idx && idx.value() < values.size());
        return values[idx.value()].get();
    }

    /// Enumerate all thread-local storages and pass them into the argument of given functor.
    template <typename TFunctor>
    void forEach(TFunctor&& functor) {
        for (auto& value : values) {
            functor(value.get());
        }
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
        pool.submit([n1, n2, &storage, &functor] {
            Type& tls = storage.get();
            functor(n1, n2, tls);
        });
    }
    pool.waitForAll();
}

NAMESPACE_SPH_END
