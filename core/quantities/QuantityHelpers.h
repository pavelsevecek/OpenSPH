#pragma once

/// \file QuantityHelpers.h
/// \brief Conversions for quantity enums
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/geometry/Tensor.h"
#include "objects/geometry/TracelessTensor.h"

NAMESPACE_SPH_BEGIN

enum class ValueEnum { SCALAR, VECTOR, TENSOR, SYMMETRIC_TENSOR, TRACELESS_TENSOR, INDEX };

/// Convert type to ValueType enum
template <typename T>
struct GetValueEnum;
template <>
struct GetValueEnum<Float> {
    static constexpr ValueEnum type = ValueEnum::SCALAR;
};
template <>
struct GetValueEnum<Vector> {
    static constexpr ValueEnum type = ValueEnum::VECTOR;
};
template <>
struct GetValueEnum<Tensor> {
    static constexpr ValueEnum type = ValueEnum::TENSOR;
};
template <>
struct GetValueEnum<SymmetricTensor> {
    static constexpr ValueEnum type = ValueEnum::SYMMETRIC_TENSOR;
};
template <>
struct GetValueEnum<TracelessTensor> {
    static constexpr ValueEnum type = ValueEnum::TRACELESS_TENSOR;
};
template <>
struct GetValueEnum<Size> {
    static constexpr ValueEnum type = ValueEnum::INDEX;
};

/// Convert ValueType enum to type
template <ValueEnum Type>
struct GetTypeFromEnum;
template <>
struct GetTypeFromEnum<ValueEnum::SCALAR> {
    using Type = Float;
};
template <>
struct GetTypeFromEnum<ValueEnum::VECTOR> {
    using Type = Vector;
};
template <>
struct GetTypeFromEnum<ValueEnum::TENSOR> {
    using Type = Tensor;
};
template <>
struct GetTypeFromEnum<ValueEnum::SYMMETRIC_TENSOR> {
    using Type = SymmetricTensor;
};
template <>
struct GetTypeFromEnum<ValueEnum::TRACELESS_TENSOR> {
    using Type = TracelessTensor;
};
template <>
struct GetTypeFromEnum<ValueEnum::INDEX> {
    using Type = Size;
};


/// \brief Selects type based on run-time ValueEnum value and runs visit<Type>() method of the visitor.
///
/// This provides a way to run generic code with different types. Return whatever TVisitor::visit returns.
template <typename TVisitor, typename... TArgs>
decltype(auto) dispatch(const ValueEnum value, TVisitor&& visitor, TArgs&&... args) {
    switch (value) {
    case ValueEnum::SCALAR:
        return visitor.template visit<Float>(std::forward<TArgs>(args)...);
    case ValueEnum::VECTOR:
        return visitor.template visit<Vector>(std::forward<TArgs>(args)...);
    case ValueEnum::TENSOR:
        return visitor.template visit<Tensor>(std::forward<TArgs>(args)...);
    case ValueEnum::SYMMETRIC_TENSOR:
        return visitor.template visit<SymmetricTensor>(std::forward<TArgs>(args)...);
    case ValueEnum::TRACELESS_TENSOR:
        return visitor.template visit<TracelessTensor>(std::forward<TArgs>(args)...);
    case ValueEnum::INDEX:
        return visitor.template visit<Size>(std::forward<TArgs>(args)...);
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
