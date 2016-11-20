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


/// Type trait wrapping reference_wrapper around l-value references. Other types keeps unchanged.
template <typename T>
struct WrapReferenceType {
    using Type = T;
};
template <typename T>
struct WrapReferenceType<T&> {
    using Type = std::reference_wrapper<T>;
};

/// Type trait for "extracting" stored references from reference_wrappers. Other types keeps unchanged.
template<typename T>
struct UnwrapReferenceType {
    using Type = T;
};
template<typename T>
struct UnwrapReferenceType<std::reference_wrapper<T>> {
    using Type = T&;
};

/// Adds const or reference to the type, based on const-ness and reference-ness of given type.
template<typename T, typename TOther>
struct UndecayType {
    using Type = T;
};

template<typename T, typename TOther>
struct UndecayType<T, const TOther> {
    using Type = const T;
};
template<typename T, typename TOther>
struct UndecayType<T, TOther&> {
    using Type = T&;
};
template<typename T, typename TOther>
struct UndecayType<T, const TOther&> {
    using Type = const T&;
};
template<typename T, typename TOther>
using Undecay = typename UndecayType<T, TOther>::Type;

static_assert(std::is_same<int, Undecay<int, float>>::value, "invalid Undecay");
static_assert(std::is_same<int&, Undecay<int, float&>>::value, "invalid Undecay");
static_assert(std::is_same<const int, Undecay<int, const float>>::value, "invalid Undecay");
static_assert(std::is_same<const int&, Undecay<int, const float&>>::value, "invalid Undecay");

/// Converts enums into int, does not change other types.
template<typename T, typename TEnabler = void>
struct EnumToIntType {
    using Type = T;
};
template<typename T>
struct EnumToIntType<T, std::enable_if_t<std::is_enum<std::decay_t<T>>::value>> {
    using Type = Undecay<std::underlying_type_t<std::decay_t<T>>, T>;
};
template<typename T>
using EnumToInt = typename EnumToIntType<T>::Type;

enum class TestEnum { DUMMY };

static_assert(std::is_same<int, EnumToInt<int>>::value, "invalid EnumToInt");
static_assert(std::is_same<float, EnumToInt<float>>::value, "invalid EnumToInt");
static_assert(std::is_same<int&, EnumToInt<int&>>::value, "invalid EnumToInt");
static_assert(std::is_same<int, EnumToInt<TestEnum>>::value, "invalid EnumToInt");
static_assert(std::is_same<int&, EnumToInt<TestEnum&>>::value, "invalid EnumToInt");

NAMESPACE_SPH_END
