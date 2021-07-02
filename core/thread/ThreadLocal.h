#pragma once

/// \file ThreadLocal.h
/// \brief Template for thread-local storage
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/Array.h"
#include "objects/utility/Iterator.h"
#include "objects/wrappers/Optional.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

namespace Detail {

struct ValueInitTag {};
struct FunctorInitTag {};

template <typename... TArgs>
struct ParamTraits {
    using Tag = ValueInitTag;
};
template <typename TArg>
struct ParamTraits<TArg> {
    using Tag = std::conditional_t<IsCallable<TArg>::value, FunctorInitTag, ValueInitTag>;
};

} // namespace Detail

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

    struct Sum {
        INLINE constexpr Type operator()(const Type& t1, const Type& t2) const {
            return t1 + t2;
        }
    };

public:
    /// \brief Constructs a thread-local storage from a list of values
    ///
    /// \param scheduler Scheduler associated with the object.
    /// \param args List of parameters that are passed into the constructor of each thread-local storage.
    template <typename... TArgs>
    ThreadLocal(IScheduler& scheduler, TArgs&&... args)
        : scheduler(scheduler) {
        initialize(typename Detail::ParamTraits<TArgs...>::Tag{}, std::forward<TArgs>(args)...);
    }

    /// \brief Constructs a thread-local storage using a functor.
    ///
    /// \param scheduler Scheduler associated with the object.
    /// \param functor Functor used to initialize each thread-local object.
    template <typename TFunctor>
    ThreadLocal(IScheduler& scheduler, TFunctor&& functor)
        : scheduler(scheduler) {
        initialize(typename Detail::ParamTraits<TFunctor>::Tag{}, std::forward<TFunctor>(functor));
    }

    /// \brief Return a value for current thread.
    ///
    /// This thread must belong the the thread pool given in constructor, checked by assert.
    INLINE Type& local() {
        const Optional<Size> idx = scheduler.getThreadIdx();
        SPH_ASSERT(idx && idx.value() < locals.size());
        return locals[idx.value()].value;
    }

    /// \copydoc local
    INLINE const Type& local() const {
        const Optional<Size> idx = scheduler.getThreadIdx();
        SPH_ASSERT(idx && idx.value() < locals.size());
        return locals[idx.value()].value;
    }

    /// \brief Returns the storage corresponding to the thread with given index.
    ///
    /// Can be called from any thread. There is no synchronization, so accessing the storage from the
    /// associated worker at the same time might cause a race condition.
    INLINE Type& value(const Size threadId) {
        return locals[threadId].value;
    }

    /// \brief Performs an accumulation of thread-local values.
    ///
    /// Uses operator + to sum up the elements.
    /// \param initial Value to which the accumulated result is initialized.
    Type accumulate(const Type& initial = Type(0._f)) const {
        return this->accumulate(initial, Sum{});
    }

    /// \brief Performs an accumulation of thread-local values.
    ///
    /// Uses provided binary predicate to accumulate the values.
    /// \param initial Value to which the accumulated result is initialized.
    /// \param predicate Callable object with signature Type operator()(const Type&, const Type&).
    template <typename TPredicate>
    Type accumulate(const Type& initial, const TPredicate& predicate) const {
        Type sum = initial;
        for (const Type& value : *this) {
            sum = predicate(sum, value);
        }
        return sum;
    }

    template <typename T>
    class LocalIterator : public Iterator<T> {
    public:
        LocalIterator(Iterator<T> iter)
            : Iterator<T>(iter) {}

        using Return = std::conditional_t<std::is_const<T>::value, const Type&, Type&>;

        INLINE Return operator*() const {
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

private:
    template <typename... TArgs>
    void initialize(Detail::ValueInitTag, TArgs&&... args) {
        const Size threadCnt = scheduler.getThreadCnt();
        locals.reserve(threadCnt);
        for (Size i = 0; i < threadCnt; ++i) {
            // intentionally not forwarded, we cannot move parameters if we have more than one object
            locals.emplaceBack(args...);
        }
    }

    template <typename TFunctor>
    void initialize(Detail::FunctorInitTag, TFunctor&& functor) {
        const Size threadCnt = scheduler.getThreadCnt();
        locals.reserve(threadCnt);
        for (Size i = 0; i < threadCnt; ++i) {
            locals.emplaceBack(functor());
        }
    }
};

/// \brief Overload of parallelFor that passes thread-local storage into the functor.
template <typename Type, typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler,
    ThreadLocal<Type>& storage,
    const Size from,
    const Size to,
    TFunctor&& functor) {
    const Size granularity = scheduler.getRecommendedGranularity();
    parallelFor(scheduler, storage, from, to, granularity, std::forward<TFunctor>(functor));
}

/// \brief Overload of parallelFor that passes thread-local storage into the functor.
template <typename Type, typename TFunctor>
INLINE void parallelFor(IScheduler& scheduler,
    ThreadLocal<Type>& storage,
    const Size from,
    const Size to,
    const Size granularity,
    TFunctor&& functor) {
    SPH_ASSERT(from <= to);

    scheduler.parallelFor(from, to, granularity, [&storage, &functor](Size n1, Size n2) {
        SPH_ASSERT(n1 < n2);
        Type& value = storage.local();
        for (Size i = n1; i < n2; ++i) {
            functor(i, value);
        }
    });
}

NAMESPACE_SPH_END
