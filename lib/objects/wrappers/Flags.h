#pragma once

/// \file Flags.h
/// \brief Wrapper over enum allowing setting (and querying) individual bits of the stored value.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Assert.h"
#include "common/Traits.h"
#include "math/MathUtils.h"

NAMESPACE_SPH_BEGIN

struct EmptyFlags {};

const EmptyFlags EMPTY_FLAGS;

template <typename TEnum>
class Flags {
private:
    using TValue = std::underlying_type_t<TEnum>;
    TValue data = TValue(0);

public:
    /// Constructs empty flags.
    constexpr Flags() = default;

    /// Constucts flags by copying other object.
    constexpr Flags(const Flags& other)
        : data(other.data) {}

    /// Constructs object given a list of flags. Every parameter must be a power of 2, checked by assert.
    template <typename... TArgs>
    constexpr Flags(const TEnum flag, const TArgs... others)
        : data(construct(flag, others...)) {}

    /// Tag to construct empty flags; use global constant EMPTY_FLAGS to construct empty Flags object.
    constexpr Flags(const EmptyFlags) {}

    /// Constructs object from underlying value. Useful when using flags as a parameter in settings. Does not
    /// check if the input value can be represented by TEnum flags, use sparingly.
    constexpr static Flags fromValue(const TValue value) {
        Flags flags;
        flags.data = value;
        return flags;
    }

    /// Copies other Flags object.
    Flags& operator=(const Flags& other) {
        data = other.data;
        return *this;
    }

    /// Assigns a single flag, all previous flags are deleted. The flag must be a power of 2.
    Flags& operator=(const TEnum flag) {
        ASSERT(isPower2(TValue(flag)));
        data = TValue(flag);
        return *this;
    }

    /// Deletes all flags.
    Flags& operator=(const EmptyFlags) {
        data = TValue(0);
        return *this;
    }

    /// Checks if the object has a given flag.
    INLINE constexpr bool has(const TEnum flag) const {
        return (data & TValue(flag)) != 0;
    }

    /// Checks if the object has any of given flags.
    template <typename... TArgs>
    INLINE constexpr bool hasAny(const TEnum flag, const TArgs... others) const {
        return has(flag) || hasAny(others...);
    }

    /// Checks if the object has all of given flags.
    template <typename... TArgs>
    INLINE constexpr bool hasAll(const TEnum flag, const TArgs... others) const {
        return has(flag) && hasAll(others...);
    }

    /// Adds a single flag into the object. All previously stored flags are kept unchanged.
    INLINE void set(const TEnum flag) {
        ASSERT(isPower2(TValue(flag)));
        data |= TValue(flag);
    }

    /// Removed a single flag. If the flag is not stored, function does nothing.
    INLINE void unset(const TEnum flag) {
        ASSERT(isPower2(TValue(flag)));
        data &= ~TValue(flag);
    }

    /// Sets or removes given flag based given boolean value.
    INLINE void setIf(const TEnum flag, const bool use) {
        ASSERT(isPower2(TValue(flag)));
        if (use) {
            data |= TValue(flag);
        } else {
            data &= ~TValue(flag);
        }
    }

    /// Returns the underlying value.
    INLINE TValue value() const {
        return data;
    }

    /// Returns a Flags object by adding a single flag to currently stored values.
    INLINE constexpr Flags operator|(const TEnum flag) {
        ASSERT(isPower2(TValue(flag)));
        Flags<TEnum> flags(*this);
        flags.set(flag);
        return flags;
    }

    /// Checks for equality with other Flags object.
    INLINE bool operator==(const Flags& other) const {
        return data == other.data;
    }

private:
    // overload with no argument ending the recursion
    INLINE constexpr bool hasAny() const {
        return false;
    }

    INLINE constexpr bool hasAll() const {
        return true;
    }

    template <typename... TArgs>
    INLINE constexpr TValue construct(const TEnum first, const TArgs... rest) {
        ASSERT(TValue(first) == 0 || isPower2(TValue(first)));
        return TValue(first) | construct(rest...);
    }

    INLINE constexpr TValue construct() {
        return 0;
    }
};

template <typename TEnum, typename = std::enable_if_t<IsEnumClass<TEnum>::value>>
INLINE constexpr Flags<TEnum> operator|(const TEnum flag1, const TEnum flag2) {
    return Flags<TEnum>(flag1, flag2);
}

NAMESPACE_SPH_END
