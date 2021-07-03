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
    bool ext = false;

public:
    ExtendedEnum() = default;

    /// Implicit constructor from base
    ExtendedEnum(const TBase value)
        : value(TValue(value))
        , ext(false) {}

    /// Implicit constructor from derived
    template <typename TDerived, typename = std::enable_if_t<IsExtended<TDerived, TBase>::value>>
    ExtendedEnum(const TDerived value)
        : value(TValue(value))
        , ext(true) {}

    /// Checks if the object currently holds extended (derived) value.
    bool extended() const {
        return ext;
    }

    /// Implicit conversion to base
    operator TBase() const {
        if (!ext) {
            return TBase(value);
        } else {
            return TBase(std::numeric_limits<TValue>::max());
        }
    }

    /// Explicit conversion to derived
    template <typename TDerived, typename = std::enable_if_t<IsExtended<TDerived, TBase>::value>>
    explicit operator TDerived() const {
        if (ext) {
            return TDerived(value);
        } else {
            using TDerivedValue = std::underlying_type_t<TBase>;
            return TDerived(std::numeric_limits<TDerivedValue>::max());
        }
    }

    bool operator<(const ExtendedEnum& other) const {
        return std::tie(ext, value) < std::tie(other.ext, other.value);
    }

    bool operator==(const ExtendedEnum& other) const {
        return ext == other.ext && value == other.value;
    }

    bool operator==(const TBase& other) const {
        return !ext && TBase(value) == other;
    }

    template <typename TDerived, typename = std::enable_if_t<IsExtended<TDerived, TBase>::value>>
    bool operator==(const TDerived& other) const {
        return ext && TDerived(value) == other;
    }

    bool operator!=(const ExtendedEnum& other) const {
        return value != other.value;
    }
};

NAMESPACE_SPH_END
