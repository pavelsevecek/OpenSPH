#pragma once

#include "common/Assert.h"
#include "common/Traits.h"
#include <limits>

NAMESPACE_SPH_BEGIN

template <typename TDerived, typename TBase>
struct IsExtended {
    static constexpr bool value = false;
};

#define SPH_EXTEND_ENUM(TDerived, TBase)                                                                     \
    template <>                                                                                              \
    struct IsExtended<TDerived, TBase> {                                                                     \
        static constexpr bool value = true;                                                                  \
    }

/// \brief Helper type allowing to "derive" from enum class.
///
/// To derive an enum, it is necessary to specialize IsExtended trait. Consider using SPH_EXTEND_ENUM macro.
template <typename TBase>
class ExtendedEnum {
private:
    using TValue = std::underlying_type_t<TBase>;
    TValue value = 0;

public:
    using BaseType = TBase;

    ExtendedEnum() = default;

    /// Implicit constructor from base
    ExtendedEnum(const TBase value)
        : value(TValue(value)) {}

    /// Implicit constructor from derived
    template <typename TDerived, typename = std::enable_if_t<IsExtended<TDerived, TBase>::value>>
    ExtendedEnum(const TDerived value)
        : value(TValue(value)) {}

    /// Implicit conversion to base
    operator TBase() const {
        return TBase(value);
    }

    /// Explicit conversion to derived
    template <typename TDerived, typename = std::enable_if_t<IsExtended<TDerived, TBase>::value>>
    explicit operator TDerived() const {
        return TDerived(value);
    }

    bool operator<(const ExtendedEnum& other) const {
        return value < other.value;
    }

    bool operator==(const ExtendedEnum& other) const {
        return value == other.value;
    }

    bool operator==(const TBase& other) const {
        return TBase(value) == other;
    }

    template <typename TDerived, typename = std::enable_if_t<IsExtended<TDerived, TBase>::value>>
    bool operator==(const TDerived& other) const {
        return TDerived(value) == other;
    }

    bool operator!=(const ExtendedEnum& other) const {
        return value != other.value;
    }
};

template <typename T>
struct IsExtendedEnum {
    static constexpr bool value = false;
};

template <typename TEnum>
struct IsExtendedEnum<ExtendedEnum<TEnum>> {
    static constexpr bool value = true;
};

NAMESPACE_SPH_END
