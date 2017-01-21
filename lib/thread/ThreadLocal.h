#pragma once

/// Thread-local storage
/// Pavel Sevecek 2017
/// sevecek@sirrah.troja.mff.cuni.cz

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

    /// Return a value for current thread. This thread must belong the the thread pool given in constructor, checked by assert.
    Type& get() {
        const Optional<Size> idx = pool.getThreadIdx();
        ASSERT(idx && idx.get() < values.size());
        return values[idx.get()].get();
    }

    /// Return a value for current thread, const version.
    const Type& get() const {
        const Optional<Size> idx = pool.getThreadIdx();
        ASSERT(idx && idx.get() < values.size());
        return values[idx.get()].get();
    }

    /// Enumerate all thread-local storages and pass them into the argument of given functor.
    template<typename TFunctor>
    void forEach(TFunctor&& functor) {
        for (auto& value :values) {
            functor(value.get());
        }
    }
};

NAMESPACE_SPH_END
