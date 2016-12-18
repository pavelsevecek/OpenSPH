#pragma once

#include "solvers/Accumulator.h"

NAMESPACE_SPH_BEGIN

namespace Detail {
    template <typename TAccumulate, typename... TOwners>
    struct OwnerIdx;
    template <typename TAccumulate, typename TOwner, typename... TOthers>
    struct OwnerIdx<TAccumulate, TOwner, TOthers...> {
        static constexpr bool isOwned = TupleContains<typename TOwner::Accumulating, TAccumulate>::value;
        static constexpr int value = isOwned ? 0 : 1 + OwnerIdx<TAccumulate, TOthers...>::value;
    };
    template <typename TAccumulate>
    struct OwnerIdx<TAccumulate> {
        static constexpr int value = 0;
    };
    template <typename TAccumulate, typename... TOwners>
    static constexpr bool IsShared = (OwnerIdx<TAccumulate, TOwners...>::value < sizeof...(TOwners));

    template <typename... TOwners>
    struct OwnerPack {};
}


/// Accumulator that can be shared between multiple objects. If (at least) on of TOwners::Accumulating tuples
/// contains type TAccumulate, accumulation is left to this owner object. Here, accumulation is simply skipped
/// and only a reference to the array kept.
/// Otherwise, SharedAccumulator is a simple wrapper of Accumulator.
///
/// Default implementation = simply derive from Accumulator
template <typename TAccumulate, typename TOwnerPack, typename TEnabler = void>
class SharedAccumulatorImpl : public Accumulator<TAccumulate> {
public:
    using Accumulator<TAccumulate>::Accumulator;

    /// We need to implement this so that both specializations have the same interface
    template <typename... Ts>
    SharedAccumulatorImpl(Ts&...) {}

    /// Are we the owner accumulator?
    static constexpr bool isOwner = true;
};

/// Specialization for shared accumulators
template <typename TAccumulate, typename... TOwners>
class SharedAccumulatorImpl<TAccumulate,
    Detail::OwnerPack<TOwners...>,
    std::enable_if_t<Detail::IsShared<TAccumulate, TOwners...>>> : public Noncopyable {
private:
    using Value = typename TAccumulate::Type;

    Array<Value>& values;

    static constexpr int ownerIdx = Detail::OwnerIdx<TAccumulate, TOwners...>::value;
    static_assert(ownerIdx < sizeof...(TOwners), "internal error in SharedAccumulator");

public:
    static constexpr bool isOwner = false;

    SharedAccumulatorImpl(TOwners&... owners)
        : values(selectNth<ownerIdx>(owners.template get<TAccumulate>()...)) {}

    void update(Storage& UNUSED(storage)) {}

    INLINE void accumulate(const int, const int, const Vector&) {}

    INLINE Array<Value>& get() { return values; }

    INLINE Value& operator[](const int idx) { return values[idx]; }
};

template <typename TAccumulate, typename... TOwners>
using SharedAccumulator = SharedAccumulatorImpl<TAccumulate, Detail::OwnerPack<TOwners...>>;

NAMESPACE_SPH_END
