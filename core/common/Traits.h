#pragma once

/// \file Traits.h
/// \brief Few non-standard type traits.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Globals.h"
#include <memory>
#include <type_traits>

NAMESPACE_SPH_BEGIN

/// Gets n-th type
template <int n, typename... TArgs>
struct TypeSelector;

template <int n, typename T0, typename... TArgs>
struct TypeSelector<n, T0, TArgs...> : TypeSelector<n - 1, TArgs...> {};

template <typename T0, typename... TArgs>
struct TypeSelector<0, T0, TArgs...> {
    using Type = T0;
};

template <int n, typename... TArgs>
using SelectType = typename TypeSelector<n, TArgs...>::Type;

/// Returns the index of type T0 in type list. If the type is not in the list, returns -1.
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


template <typename... Ts>
struct MakeVoid {
    using Type = void;
};
template <typename... Ts>
using VoidType = typename MakeVoid<Ts...>::Type;

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
    using TSignature = typename FunctionSignature<TReturn(TArgs...)>::T;
    using TReturnType = TReturn;
};

template <typename TClass, typename TReturn, typename... TArgs>
struct FunctionTraits<TReturn (TClass::*)(TArgs...)> {
    using TSignature = typename FunctionSignature<TReturn(TArgs...)>::T;
    using TReturnType = TReturn;
};

template <typename TReturn, typename... TArgs>
struct FunctionTraits<TReturn (*)(TArgs...)> {
    using TSignature = typename FunctionSignature<TReturn(TArgs...)>::T;
    using TReturnType = TReturn;
};

template <typename TFunction>
using ReturnType = typename FunctionTraits<TFunction>::TReturnType;


template <typename TCallable, typename TEnabler, typename... TArgs>
struct IsCallableImpl {
    static constexpr bool value = false;
};

template <typename TCallable, typename... TArgs>
struct IsCallableImpl<TCallable,
    VoidType<decltype(std::declval<TCallable>()(std::declval<TArgs>()...))>,
    TArgs...> {
    static constexpr bool value = true;
};
template <typename TCallable, typename... TArgs>
struct IsCallable {
    static constexpr bool value = IsCallableImpl<TCallable, void, TArgs...>::value;
};


template <typename T, typename TStream, typename = void>
struct HasStreamOperator {
    static constexpr bool value = false;
};

template <typename T, typename TStream>
struct HasStreamOperator<T, TStream, VoidType<decltype(std::declval<TStream&>() << std::declval<T>())>> {
    static constexpr bool value = true;
};


/// Helper class for storing l-value references. Has a default constructor for convenient usage in containers.
template <typename T>
class ReferenceWrapper {
private:
    T* data = nullptr;

public:
    ReferenceWrapper() = default;

    ReferenceWrapper(const ReferenceWrapper& other)
        : data(other.data) {}

    ReferenceWrapper(T& ref)
        : data(std::addressof(ref)) {}

    ReferenceWrapper& operator=(const ReferenceWrapper& other) {
        data = other.data;
        return *this;
    }

    INLINE operator T&() noexcept {
        return *data;
    }

    INLINE operator const T&() const noexcept {
        return *data;
    }
};


/// Type trait wrapping references. Other types keeps unchanged.
template <typename T>
struct WrapReferenceType {
    using Type = T;
};
template <typename T>
struct WrapReferenceType<T&> {
    using Type = ReferenceWrapper<T>;
};
template <typename T>
using WrapReference = typename WrapReferenceType<T>::Type;


/// Type trait for "extracting" stored references from reference_wrappers. Other types keeps unchanged.
template <typename T>
struct UnwrapReferenceType {
    using Type = T;
};
template <typename T>
struct UnwrapReferenceType<ReferenceWrapper<T>> {
    using Type = T&;
};

/// Adds const or reference to the type, based on const-ness and reference-ness of given type.
template <typename T, typename TOther>
struct UndecayType {
    using Type = T;
};

template <typename T, typename TOther>
struct UndecayType<T, const TOther> {
    using Type = const T;
};
template <typename T, typename TOther>
struct UndecayType<T, TOther&> {
    using Type = T&;
};
template <typename T, typename TOther>
struct UndecayType<T, const TOther&> {
    using Type = const T&;
};
template <typename T, typename TOther>
using Undecay = typename UndecayType<T, TOther>::Type;

/// Converts all signed integral types and enums into Size, does not change other types.
template <typename T, typename TEnabler = void>
struct ConvertToSizeType {
    using Type = T;
};
template <typename T>
struct ConvertToSizeType<T, std::enable_if_t<std::is_enum<std::decay_t<T>>::value>> {
    using Type = Undecay<std::underlying_type_t<std::decay_t<T>>, T>;
};
template <typename T>
using ConvertToSize = typename ConvertToSizeType<T>::Type;


template <typename T>
struct IsEnumClass {
    static constexpr bool value = std::is_enum<T>::value && !std::is_convertible<T, int>::value;
};


/// Static logical and
/// \todo Can be replaced by C++17 fold expressions
template <bool... Values>
struct AllTrue;
template <bool First, bool Second, bool... Others>
struct AllTrue<First, Second, Others...> {
    static constexpr bool value = First && AllTrue<Second, Others...>::value;
};
template <bool Value>
struct AllTrue<Value> {
    static constexpr bool value = Value;
};

/// Static logical or
template <bool... Values>
struct AnyTrue;
template <bool First, bool Second, bool... Others>
struct AnyTrue<First, Second, Others...> {
    static constexpr bool value = First || AnyTrue<Second, Others...>::value;
};
template <bool Value>
struct AnyTrue<Value> {
    static constexpr bool value = Value;
};


/// \brief Converts a non-const reference to const one.
///
/// Similar to std::as_const in C++17. Useful for unit testing to explicitly call a const overload of a
/// function, or for some hacky tricks with const_cast.
template <typename T>
INLINE const T& asConst(T& ref) {
    return ref;
}

template <typename T>
INLINE const T& asConst(const T&& ref) = delete;

NAMESPACE_SPH_END
