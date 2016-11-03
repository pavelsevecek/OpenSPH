#pragma once

/// Wrapper over enum allowing setting (and querying) individual bits of the stored value.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

struct EmptyFlags : public Object {};

const EmptyFlags EMPTY_FLAGS;

template <typename TEnum>
class Flags : public Object {
private:
    using TValue = std::underlying_type_t<TEnum>;
    TValue data  = TValue(0);

public:
    constexpr Flags() = default;

    constexpr Flags(const Flags& other)
        : data(other.data) {}

    constexpr Flags(const TEnum& flag)
        : data(TValue(flag)) {}

    constexpr Flags(const EmptyFlags&) {}

    Flags& operator=(const Flags& other) {
        data = other.data;
        return *this;
    }

    Flags& operator=(const TEnum& flag) {
        data = TValue(flag);
        return *this;
    }

    Flags& operator=(const EmptyFlags&) {
        data = TValue(0);
        return *this;
    }

    INLINE constexpr bool has(const TEnum& flag) const { return (data & TValue(flag)) != 0; }

    INLINE void set(const TEnum& flag) { data |= TValue(flag); }
};


NAMESPACE_SPH_END
