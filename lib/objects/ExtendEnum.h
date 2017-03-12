#pragma once

/// Helper type allowing to extend enum, essentially merging two or more enums while providing explicit
/// conversion to underlying type. Note that there is no check that enums do not overlap, this is a
/// responsibility of the user. It's also possible to convert value of one enum to value of another enum,
/// object holds no record what enum type is currently stored.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "common/Traits.h"
#include "objects/Object.h"

NAMESPACE_SPH_BEGIN


template <typename... TEnums>
class ExtendEnum {
private:
    Size value;

public:
    ExtendEnum() = default;

    template <typename T, typename = std::enable_if_t<getTypeIndex<T, TEnums...> != -1>>
    ExtendEnum(const T value)
        : value(Size(value)) {}

    explicit ExtendEnum(const int value)
        : value(value) {}

    ExtendEnum(const ExtendEnum& other)
        : value(other.value) {}

    INLINE operator Size() const {
        return value;
    }

    template <typename T, typename = std::enable_if_t<getTypeIndex<T, TEnums...> != -1>>
    INLINE operator T() const {
        return T(value);
    }

    template <typename T, typename = std::enable_if_t<getTypeIndex<T, TEnums...> != -1>>
    friend bool operator==(const ExtendEnum lhs, const T rhs) {
        return lhs.value == Size(rhs);
    }

    template <typename T, typename = std::enable_if_t<getTypeIndex<T, TEnums...> != -1>>
    friend bool operator==(const T lhs, const ExtendEnum rhs) {
        return Size(lhs) == rhs.value;
    }
};

/// \todo either use int or Size everywhere
template <typename... TArgs>
struct ConvertToSizeType<ExtendEnum<TArgs...>> {
    using Type = int;
};
template <typename... TArgs>
struct ConvertToSizeType<ExtendEnum<TArgs...>&> {
    using Type = int;
};
template <typename... TArgs>
struct ConvertToSizeType<const ExtendEnum<TArgs...>> {
    using Type = int;
};
template <typename... TArgs>
struct ConvertToSizeType<const ExtendEnum<TArgs...>&> {
    using Type = int;
};


NAMESPACE_SPH_END
