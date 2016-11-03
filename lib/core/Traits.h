#pragma once

/// Few non-standard type traits.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <type_traits>

NAMESPACE_SPH_BEGIN

/// Gets n-th type
template <int n, typename... TArgs>
struct TypeSelector;

template <int n, typename T0, typename... TArgs>
struct TypeSelector<n, T0, TArgs...> : TypeSelector<n - 1, TArgs...> {};

template <typename T0, typename... TArgs>
struct TypeSelector<0, T0, TArgs...> : public Object {
    using Type = T0;
};

template <int n, typename... TArgs>
using SelectType = typename TypeSelector<n, TArgs...>::Type;

/// Returns the index of type
template <typename T0, int n, typename T1, typename... TArgs>
struct TypeIndex {
    static constexpr int value = std::is_same<T0, T1>::value ? n : TypeIndex<T0, n + 1, TArgs...>::value;
};

template <typename T0, int n, typename T1>
struct TypeIndex<T0, n, T1> {
    static constexpr int value = std::is_same<T0, T1>::value ? n : -1;
};

template <typename T0, typename... TArgs>
constexpr int getTypeIndex = TypeIndex<T0, 0, TArgs...>::value;


/// Trait to determine whether the type is a struct holding a single value with (at least explicit) conversion
/// and cast operators defined. False by default, need to be specialized if one wishes to use certain struct
/// (like units) instead of ordinary floating-point variables in algorithms.
template <typename T>
struct IsValueType {
    static constexpr bool value = false;
};
template <>
struct IsValueType<float> {
    static constexpr bool value = true;
};
template <>
struct IsValueType<double> {
    static constexpr bool value = true;
};
template <typename T>
constexpr bool IsValue = IsValueType<T>::value;

/// Trait to check if the type is a value (see IsValueType) AND if the base type of the value has given
/// precision.
template <typename T, typename TPrecision>
struct IsValueOfPrecisionType {
    static constexpr bool value = false;
};
template <>
struct IsValueOfPrecisionType<float, float> {
    static constexpr bool value = true;
};
template <>
struct IsValueOfPrecisionType<double, double> {
    static constexpr bool value = true;
};
template <typename T, typename TPrecision>
constexpr bool IsValueOfPrecision = IsValueOfPrecisionType<T, TPrecision>::value;


/// Base type of the quantity
/// By default, it is the type of the quantity itself
/// For units, it is the type of the value.
template <typename TUnit>
struct BaseType {
    using Type = TUnit;
};

template <typename T>
using Base = typename BaseType<T>::Type;

/// A test if the the type T has a base TBase
template <typename T, typename TBase, typename = void>
struct IsBaseType {
    static constexpr bool value = false;
};

template <typename T, typename TBase>
struct IsBaseType<T, TBase, std::enable_if_t<std::is_same<Base<T>, TBase>::value>> {
    static constexpr bool value = true;
};

template <typename T, typename TBase>
constexpr bool IsBase = IsBaseType<T, TBase>::value;

/// A test if all types have the same common base TBase
template <typename TBase, typename... TArgs>
struct IsBaseAllType {};

template <typename TBase, typename T0, typename... TArgs>
struct IsBaseAllType<TBase, T0, TArgs...> {
    static constexpr bool value = IsBaseAllType<TBase, TArgs...>::value && IsBase<T0, TBase>;
};

template <typename TBase, typename T0>
struct IsBaseAllType<TBase, T0> {
    static constexpr bool value = IsBase<T0, TBase>;
};

template <typename TBase, typename... TArgs>
constexpr bool IsBaseAll = IsBaseAllType<TBase, TArgs...>::value;

/// Useful types for functions that change the dimensions of units
/// By default, it is simply the type itself

/// Product of two quantities
template <typename T1, typename T2>
struct ProductType {
    using Type = decltype(std::declval<T1>() * std::declval<T2>());
};


/// Ratio of two quantities
template <typename T1, typename T2>
struct QuotientType {
    using Type = decltype(std::declval<T1>() / std::declval<T2>());
};


/// n-th power of a quantity
template <typename T, int n>
struct PowerType {
    using Type = T;
};

/// n-th root of a quantity
template <typename T, int n>
struct RootType {
    using Type = T;
};


/// Change the base type of a quantity
template <typename T, typename U>
struct ChangePrecisionType {
    using Type = U;
};


/// Aliases
template <typename T1, typename T2>
using Product = typename ProductType<T1, T2>::Type;

template <typename T1, typename T2>
using Quotient = typename QuotientType<T1, T2>::Type;

template <typename T, int n>
using Power = typename PowerType<T, n>::Type;

template <typename T>
using Inverted = typename PowerType<T, -1>::Type;

template <typename T, int n>
using Root = typename RootType<T, n>::Type;

template <typename T, typename U>
using ChangePrecision = typename ChangePrecisionType<T, U>::Type;

/// Is the type a n-th power?
template <typename T, int n>
struct IsPowerType {
    static constexpr bool value = true;
};
template <typename T, int n>
constexpr bool IsPower = IsPowerType<T, n>::value;

/// Is dimensionless?
template <typename T>
struct IsDimensionlessType {
    static constexpr bool value = true;
};
template <typename T>
constexpr bool IsDimensionless = IsDimensionlessType<T>::value;

/// Is angular unit (degrees or radians)?
template <typename T>
struct IsAngularType {
    static constexpr bool value = true;
};
template <typename T>
constexpr bool IsAngular = IsAngularType<T>::value;

/// Type traits

template <typename... TArgs>
using TraitHelper = void;

/// Can we get a product of T1 and T2?
template <typename T1, typename T2, typename TEnabler = void>
struct HaveProductType {
    static constexpr bool value = false;
};

template <typename T1, typename T2>
struct HaveProductType<T1, T2, TraitHelper<decltype(std::declval<T1>() * std::declval<T2>())>> {
    static constexpr bool value = true;
};

template <typename T1, typename T2>
constexpr bool HaveProduct = HaveProductType<T1, T2>::value;

/// Can we get a quotient of T1 and T2?
template <typename T1, typename T2, typename TEnabler = void>
struct HaveQuotientType {
    static constexpr bool value = false;
};

template <typename T1, typename T2>
struct HaveQuotientType<T1, T2, TraitHelper<decltype(std::declval<T1>() / std::declval<T2>())>> {
    static constexpr bool value = true;
};

template <typename T1, typename T2>
constexpr bool HaveQuotient = HaveQuotientType<T1, T2>::value;

// tests
static_assert(HaveProduct<int, float>, "Static test failed");
static_assert(HaveProduct<float, double>, "Static test failed");
static_assert(HaveProduct<float&, double>, "Static test failed");
struct StructNoProduct {};
static_assert(!HaveProduct<float, StructNoProduct>, "Static test failed");
struct StructWithProduct {
    float operator*(const float f) { return f; }
};
static_assert(HaveProduct<StructWithProduct, float>, "Static test failed");
static_assert(HaveProduct<StructWithProduct&, float>, "Static test failed");
static_assert(!HaveProduct<StructWithProduct*, float>, "Static test failed");
static_assert(!HaveProduct<StructWithProduct, StructWithProduct>, "Static test failed");

static_assert(HaveQuotient<int, float>, "Static test failed");
struct StructWithQuotient {
    float operator/(const float f) { return f; }
};
static_assert(HaveQuotient<StructWithQuotient, float>, "Static test failed");
static_assert(!HaveQuotient<float, StructWithQuotient>, "Static test failed");
static_assert(!HaveQuotient<StructWithQuotient, StructWithQuotient>, "Static test failed");

/// Function traits

template <typename TSignature>
struct FunctionSignature {
    using T = TSignature;
};

template <typename TFunction>
struct FunctionTraits : public FunctionTraits<decltype(&TFunction::operator())> {
    using TSignature = typename FunctionTraits<decltype(&TFunction::operator())>::TSignature;
};

template <typename TClass, typename TReturn, typename... TArgs>
struct FunctionTraits<TReturn (TClass::*)(TArgs...) const> {
    using TSignature  = typename FunctionSignature<TReturn(TArgs...)>::T;
    using TReturnType = TReturn;
};

template <typename TClass, typename TReturn, typename... TArgs>
struct FunctionTraits<TReturn (TClass::*)(TArgs...)> {
    using TSignature  = typename FunctionSignature<TReturn(TArgs...)>::T;
    using TReturnType = TReturn;
};

template <typename TReturn, typename... TArgs>
struct FunctionTraits<TReturn (*)(TArgs...)> {
    using TSignature  = typename FunctionSignature<TReturn(TArgs...)>::T;
    using TReturnType = TReturn;
};

template <typename TFunction>
using ReturnType = typename FunctionTraits<TFunction>::TReturnType;


/// Type traits
template <typename T>
struct NestedType {
    using Type = T;
};

template <typename T, template <typename TT> typename TStorage>
struct NestedType<TStorage<T>> {
    using Type = typename NestedType<T>::Type;
};

template <typename T>
using GetNestedType = typename NestedType<T>::Type;

// tests
template <typename T>
struct DummyTemplatedStruct {};
static_assert(std::is_same<float, GetNestedType<DummyTemplatedStruct<float>>>::value, "static test failed");
static_assert(std::is_same<float, GetNestedType<DummyTemplatedStruct<DummyTemplatedStruct<float>>>>::value,
              "static test failed");

NAMESPACE_SPH_END
