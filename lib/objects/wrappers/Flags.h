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
    TValue data = TValue(0);

public:
    constexpr Flags() = default;

    constexpr Flags(const Flags& other)
        : data(other.data) {}

    template <typename... TArgs>
    constexpr Flags(const TEnum flag, const TArgs... others)
        : data(construct(flag, others...)) {}

    constexpr Flags(const EmptyFlags) {}

    Flags& operator=(const Flags& other) {
        data = other.data;
        return *this;
    }

    Flags& operator=(const TEnum flag) {
        data = TValue(flag);
        return *this;
    }

    Flags& operator=(const EmptyFlags) {
        data = TValue(0);
        return *this;
    }

    INLINE constexpr bool has(const TEnum flag) const { return (data & TValue(flag)) != 0; }

    template <typename... TArgs>
    INLINE constexpr bool hasAny(const TEnum flag, const TArgs... others) const {
        return has(flag) || hasAny(others...);
    }

    INLINE void set(const TEnum flag) { data |= TValue(flag); }

    INLINE void unset(const TEnum flag) { data &= ~TValue(flag); }

    INLINE void setIf(const TEnum flag, const bool use) {
        if (use) {
            data |= TValue(flag);
        } else {
            data &= ~TValue(flag);
        }
    }

    INLINE constexpr Flags<TEnum> operator|(const TEnum flag) { return Flags<TEnum>(TEnum(data), flag); }

private:
    // overload with no argument ending the recursion
    INLINE constexpr bool hasAny() const { return false; }

    template <typename... TArgs>
    INLINE constexpr TValue construct(const TEnum first, const TArgs... rest) {
        return TValue(first) | construct(rest...);
    }

    INLINE constexpr TValue construct() { return 0; }
};

template <typename TEnum, typename = std::enable_if_t<std::is_enum<TEnum>::value>>
INLINE constexpr Flags<TEnum> operator|(const TEnum flag1, const TEnum flag2) {
    return Flags<TEnum>(flag1, flag2);
}

NAMESPACE_SPH_END
