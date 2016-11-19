#pragma once

#include "geometry/Tensor.h"

NAMESPACE_SPH_BEGIN

/// \todo maybe split into levels and number of derivatives?
enum class TemporalEnum {
    CONST        = 1 << 0, ///< Quantity without derivatives, or "zero order" of quantity
    FIRST_ORDER  = 1 << 1, ///< Quantity with 1st derivative
    SECOND_ORDER = 1 << 2, ///< Quantity with 1st and 2nd derivative
    /// additional helper flags for getters
    ALL           = 1 << 3, ///< All values and derivatives of all quantities
    HIGHEST_ORDER = 1 << 4, ///< Derivative with the highest order; nothing for const quantities
};

enum class ValueEnum { SCALAR, VECTOR, TENSOR, TRACELESS_TENSOR };



/// Convert type to ValueType enum
template <typename T>
struct GetValueType;
template <>
struct GetValueType<Float> {
    static constexpr ValueEnum type = ValueEnum::SCALAR;
};
template <>
struct GetValueType<Vector> {
    static constexpr ValueEnum type = ValueEnum::VECTOR;
};
template <>
struct GetValueType<Tensor> {
    static constexpr ValueEnum type = ValueEnum::TENSOR;
};

/// Convert ValueType enum to type
template <ValueEnum Type>
struct GetTypeFromValue;
template <>
struct GetTypeFromValue<ValueEnum::SCALAR> {
    using Type = Float;
};
template <>
struct GetTypeFromValue<ValueEnum::VECTOR> {
    using Type = Vector;
};
template <>
struct GetTypeFromValue<ValueEnum::TENSOR> {
    using Type = Tensor;
};

/// Selects type based on run-time ValueEnum value and runs visit<Type>() method of the visitor. This provides
/// a way to run generic code with different types. Return whatever TVisitor::visit returns.
template <typename TVisitor, typename... TArgs>
auto dispatch(const ValueEnum value, TVisitor&& visitor, TArgs&&... args) {
    switch (value) {
    case ValueEnum::SCALAR:
        return visitor.template visit<Float>(std::forward<TArgs>(args)...);
    case ValueEnum::VECTOR:
        return visitor.template visit<Vector>(std::forward<TArgs>(args)...);
    case ValueEnum::TENSOR:
        return visitor.template visit<Tensor>(std::forward<TArgs>(args)...);
    default:
        NOT_IMPLEMENTED;
    }
}


NAMESPACE_SPH_END
