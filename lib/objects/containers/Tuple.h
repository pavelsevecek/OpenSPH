#pragma once

/// \file Tuple.h
/// \brief Re-implementation of std::tuple with some additional functionality.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "common/Traits.h"

NAMESPACE_SPH_BEGIN

namespace Detail {
template <std::size_t TIndex, typename TValue>
class TupleValue {
    TValue value;

public:
    constexpr TupleValue() = default;

    template <typename T>
    INLINE constexpr TupleValue(T&& value)
        : value(std::forward<T>(value)) {}

    template <typename T>
    INLINE constexpr TupleValue& operator=(T&& t) {
        value = std::forward<T>(t);
        return *this;
    }

    using ReturnType = std::conditional_t<std::is_rvalue_reference<TValue>::value, TValue&&, TValue&>;

    /// Return r-value reference if TValue is an r-value reference, otherwise returns l-value reference.
    INLINE constexpr ReturnType get() {
        return static_cast<ReturnType>(value);
    }

    /// Return const r-value reference if TValue is an r-value reference, otherwise returns const l-value
    /// reference.
    /// \todo Does this make any sense? What's the point of const r-value reference?
    INLINE constexpr const TValue& get() const {
        return value;
    }

    /// 'Forwards' the value out of tuple, i.e. returns l-value reference for l-values and r-value
    /// reference otherwise.
    INLINE constexpr TValue&& forward() {
        return std::forward<TValue>(value);
    }
};

template <typename TSequence, typename... TArgs>
class TupleImpl;

template <std::size_t... TIndices, typename... TArgs>
class TupleImpl<std::index_sequence<TIndices...>, TArgs...> : TupleValue<TIndices, TArgs>... {
private:
    static_assert(sizeof...(TIndices) == sizeof...(TArgs), "Internal error in tuple");

    template <std::size_t TIndex>
    using Value = TupleValue<TIndex, SelectType<TIndex, TArgs...>>;

public:
    constexpr TupleImpl() = default;

    template <typename... Ts,
        typename = std::enable_if_t<AllTrue<std::is_constructible<TArgs, Ts>::value...>::value>>
    INLINE constexpr TupleImpl(Ts&&... args)
        : TupleValue<TIndices, TArgs>(std::forward<Ts>(args))... {}

    template <typename... Ts>
    INLINE constexpr TupleImpl(const TupleImpl<Ts...>& other)
        : TupleValue<TIndices, TArgs>(other.template get<TIndices>())... {}

    template <typename... Ts>
    INLINE constexpr TupleImpl(TupleImpl<Ts...>&& other)
        : TupleValue<TIndices, TArgs>(other.template forward<TIndices>())... {}

    template <std::size_t TIndex>
    INLINE constexpr decltype(auto) get() {
        return Value<TIndex>::get();
    }

    template <std::size_t TIndex>
    INLINE constexpr decltype(auto) get() const {
        return Value<TIndex>::get();
    }

    template <std::size_t TIndex>
    INLINE constexpr decltype(auto) forward() {
        return Value<TIndex>::forward();
    }

protected:
    template <typename... Ts, std::size_t TIndex, std::size_t... TIdxs>
    INLINE void copyAssign(const TupleImpl<Ts...>& other, std::index_sequence<TIndex, TIdxs...>) {
        this->get<TIndex>() = other.template get<TIndex>();
        copyAssign(other, std::index_sequence<TIdxs...>());
    }

    template <typename... Ts>
    INLINE void copyAssign(const TupleImpl<Ts...>& UNUSED(other), std::index_sequence<>) {}

    template <typename... Ts, std::size_t TIndex, std::size_t... TIdxs>
    INLINE void copyAssign(TupleImpl<Ts...>& other, std::index_sequence<TIndex, TIdxs...>) {
        this->get<TIndex>() = other.template get<TIndex>();
        copyAssign(other, std::index_sequence<TIdxs...>());
    }

    template <typename... Ts>
    INLINE void copyAssign(TupleImpl<Ts...>& UNUSED(other), std::index_sequence<>) {}

    template <typename... Ts, std::size_t TIndex, std::size_t... TIdxs>
    INLINE void moveAssign(TupleImpl<Ts...>&& other, std::index_sequence<TIndex, TIdxs...>) {
        this->get<TIndex>() = other.template forward<TIndex>();
        moveAssign(std::move(other), std::index_sequence<TIdxs...>());
    }

    template <typename... Ts>
    INLINE void moveAssign(const TupleImpl<Ts...>& UNUSED(other), std::index_sequence<>) {}

    template <typename... Ts, std::size_t TIndex, std::size_t... TIdxs>
    INLINE bool isEqual(const TupleImpl<Ts...>& other, std::index_sequence<TIndex, TIdxs...>) const {
        return this->get<TIndex>() == other.template get<TIndex>() &&
               isEqual(other, std::index_sequence<TIdxs...>());
    }

    template <typename... Ts>
    INLINE bool isEqual(const TupleImpl<Ts...>& UNUSED(other), std::index_sequence<>) const {
        return true;
    }
};
} // namespace Detail

/// Useful type trait, checking whether given type is a Tuple
template <typename T>
struct IsTuple {
    static constexpr bool value = false;
};


/// \brief Heterogeneous container capable of storing a fixed number of values.
///
/// Can store any copy-constructible or move-constructible type, l-value references and r-value reference.
/// Empty tuples are allowed. Only tuples of size 1 containing other tuple are forbidden, for simplicity.
template <typename... TArgs>
class Tuple : public Detail::TupleImpl<std::index_sequence_for<TArgs...>, TArgs...> {
private:
    using Sequence = std::index_sequence_for<TArgs...>;
    using Impl = Detail::TupleImpl<Sequence, TArgs...>;

public:
    INLINE constexpr Tuple() = default;

    template <typename... Ts,
        typename = std::enable_if_t<AllTrue<std::is_constructible<TArgs, Ts>::value...>::value>>
    INLINE constexpr Tuple(Ts&&... args)
        : Impl(std::forward<Ts>(args)...) {}

    INLINE constexpr Tuple(const Tuple& other)
        : Impl(other) {}

    INLINE constexpr Tuple(Tuple&& other)
        : Impl(std::move(other)) {}

    template <typename... Ts>
    INLINE constexpr Tuple(const Tuple<Ts...>& other)
        : Impl(other) {}

    template <typename... Ts>
    INLINE constexpr Tuple(Tuple<Ts...>&& other)
        : Impl(std::move(other)) {}

    /// Assign conts l-value reference of tuple. All values are copied.
    INLINE Tuple& operator=(const Tuple& other) {
        this->copyAssign(other, Sequence());
        return *this;
    }

    /// Assign conts l-value reference of tuple. If rhs contains l-value references, their values are copied,
    /// otherwise values are moved.
    INLINE Tuple& operator=(Tuple&& other) {
        this->copyAssign(std::move(other), Sequence());
        return *this;
    }

    /// Assigns tuple of generally different types. Same rules as above apply.
    template <typename... Ts>
    INLINE Tuple& operator=(const Tuple<Ts...>& other) {
        static_assert(sizeof...(Ts) == sizeof...(TArgs), "Cannot assign tuples of different sizes");
        this->copyAssign(other, Sequence());
        return *this;
    }

    /// Assigns tuple of generally different types. Same rules as above apply.
    template <typename... Ts>
    INLINE Tuple& operator=(Tuple<Ts...>&& other) {
        static_assert(sizeof...(Ts) == sizeof...(TArgs), "Cannot assign tuples of different sizes");
        this->moveAssign(std::move(other), Sequence());
        return *this;
    }

    /// Returns an element of the tuple by index. If a value or l-value reference is stored, an l-value
    /// reference to the element is returned. If an r-value references is stored, r-value reference is
    /// returned.
    template <std::size_t TIndex>
    INLINE constexpr decltype(auto) get() & {
        static_assert(unsigned(TIndex) < sizeof...(TArgs), "Index out of bounds.");
        return Impl::template get<TIndex>();
    }

    /// Returns an element of the tuple by index, const version.
    template <std::size_t TIndex>
    INLINE constexpr decltype(auto) get() const& {
        static_assert(unsigned(TIndex) < sizeof...(TArgs), "Index out of bounds.");
        return Impl::template get<TIndex>();
    }

    /// Returns an element of r-value reference to tuple by index. If a value or r-value reference is stored,
    /// an r-value reference is returned, for stored l-value reference, l-value reference is returned (same
    /// logic as in std::forward).
    template <std::size_t TIndex>
    INLINE constexpr decltype(auto) get() && {
        static_assert(unsigned(TIndex) < sizeof...(TArgs), "Index out of bounds.");
        return Impl::template forward<TIndex>();
    }

    /// Returns an element of the tuple by type. If the tuple contains more elements of the same type, the
    /// first one is returned. The rules for l/r-value reference are same as for returning by index.
    template <typename Type>
    INLINE constexpr decltype(auto) get() & {
        constexpr std::size_t index = getTypeIndex<Type, TArgs...>;
        static_assert(index != -1, "Type not stored in tuple");
        return Impl::template get<index>();
    }

    /// Returns an element of the tuple by type, const version.
    template <typename Type>
    INLINE constexpr decltype(auto) get() const& {
        constexpr std::size_t index = getTypeIndex<Type, TArgs...>;
        static_assert(index != -1, "Type not stored in tuple");
        return Impl::template get<index>();
    }

    /// Returns an element of r-value reference to tuple. Same rules apply as for other overloads.
    template <typename Type>
    INLINE constexpr decltype(auto) get() && {
        constexpr std::size_t index = getTypeIndex<Type, TArgs...>;
        static_assert(index != -1, "Type not stored in tuple");
        return Impl::template forward<index>();
    }

    /// Returns the number of elements in tuple.
    INLINE static constexpr std::size_t size() noexcept {
        return sizeof...(TArgs);
    }

    /// \todo Generalize operator to allow comparing tuples of different types.
    INLINE constexpr bool operator==(const Tuple& other) const {
        return Impl::isEqual(other, Sequence());
    }

    INLINE constexpr bool operator!=(const Tuple& other) const {
        return !Impl::isEqual(other, Sequence());
    }
};

template <typename... TArgs>
struct IsTuple<Tuple<TArgs...>&> {
    static constexpr bool value = true;
};
template <typename... TArgs>
struct IsTuple<const Tuple<TArgs...>&> {
    static constexpr bool value = true;
};
template <typename... TArgs>
struct IsTuple<Tuple<TArgs...>&&> {
    static constexpr bool value = true;
};


/// Creates a tuple from a pack of values, utilizing type deduction.
template <typename... TArgs>
INLINE auto makeTuple(TArgs&&... args) {
    return Tuple<std::decay_t<TArgs>...>(std::forward<TArgs>(args)...);
}

/// Creates a tuple of l-value references. Use IGNORE placeholder to omit one or more parameters.
template <typename... TArgs>
INLINE Tuple<TArgs&...> tieToTuple(TArgs&... args) {
    return Tuple<TArgs&...>(args...);
}

/// Creates a tuple of l-value or r-value references, following perfect forwarding semantics. Value returned
/// from tuple using get() will then have the same type as the input parameter. Same goes for values passed
/// into forEach functor.
template <typename... TArgs>
INLINE Tuple<TArgs&&...> forwardAsTuple(TArgs&&... args) {
    return Tuple<TArgs&&...>(std::forward<TArgs>(args)...);
}

/// Placeholder for unused variables in tieToTuple
const struct Ignore {
    template <typename T>
    const Ignore& operator=(T&&) const {
        return *this;
    }
} IGNORE;

namespace Detail {
template <typename... Ts1, typename... Ts2, std::size_t... TIdxs>
INLINE constexpr Tuple<Ts1..., Ts2...> appendImpl(const Tuple<Ts1...>& tuple,
    std::index_sequence<TIdxs...>,
    Ts2&&... values) {
    return Tuple<Ts1..., Ts2...>(tuple.template get<TIdxs>()..., std::forward<Ts2>(values)...);
}

template <typename... Ts1, typename... Ts2, std::size_t... TIdxs>
INLINE constexpr Tuple<Ts1..., Ts2...> appendImpl(Tuple<Ts1...>&& tuple,
    std::index_sequence<TIdxs...>,
    Ts2&&... values) {
    return Tuple<Ts1..., Ts2...>(std::move(tuple).template get<TIdxs>()..., std::forward<Ts2>(values)...);
}

template <typename... Ts1, typename... Ts2, std::size_t... TIdxs1, std::size_t... TIdxs2>
INLINE constexpr Tuple<Ts1..., Ts2...> appendImpl(const Tuple<Ts1...>& t1,
    const Tuple<Ts2...>& t2,
    std::index_sequence<TIdxs1...>,
    std::index_sequence<TIdxs2...>) {
    return Tuple<Ts1..., Ts2...>(t1.template get<TIdxs1>()..., t2.template get<TIdxs2>()...);
}
} // namespace Detail

/// Creates a tuple by appending values into an existing tuple. If the input tuple contains l-value
/// references, only the reference is copied, otherwise elements of new tuple are copy-constructed.
template <typename... Ts1, typename... Ts2>
INLINE constexpr Tuple<Ts1..., Ts2...> append(const Tuple<Ts1...>& tuple, Ts2&&... values) {
    return Detail::appendImpl(tuple, std::index_sequence_for<Ts1...>(), std::forward<Ts2>(values)...);
}

/// Creates a tuple by moving elements of existing tuple and appending pack of values.
template <typename... Ts1, typename... Ts2>
INLINE constexpr Tuple<Ts1..., Ts2...> append(Tuple<Ts1...>&& tuple, Ts2&&... values) {
    return Detail::appendImpl(
        std::move(tuple), std::index_sequence_for<Ts1...>(), std::forward<Ts2>(values)...);
}

/// Creates elements of two tuples.
template <typename... Ts1, typename... Ts2>
INLINE constexpr Tuple<Ts1..., Ts2...> append(const Tuple<Ts1...>& t1, const Tuple<Ts2...>& t2) {
    return Detail::appendImpl(t1, t2, std::index_sequence_for<Ts1...>(), std::index_sequence_for<Ts2...>());
}

/// Combile-time check if a tuple contains given type (at least once). Types T, T&, const T etc. are
/// considered different.
template <typename TTuple, typename Type>
struct TupleContains {
    static constexpr bool value = false;
};
template <typename... TArgs, typename Type>
struct TupleContains<Tuple<TArgs...>, Type> {
    static constexpr bool value = (getTypeIndex<Type, TArgs...> != -1);
};

/// Gets type of tuple element given its index.
template <typename TTuple, std::size_t TIndex>
struct TupleElementType;
template <typename... TArgs, std::size_t TIndex>
struct TupleElementType<Tuple<TArgs...>, TIndex> {
    using Type = SelectType<TIndex, TArgs...>;
};
template <typename... TArgs, std::size_t TIndex>
struct TupleElementType<Tuple<TArgs...>&, TIndex> {
    using Type = SelectType<TIndex, TArgs...>;
};
template <typename... TArgs, std::size_t TIndex>
struct TupleElementType<const Tuple<TArgs...>&, TIndex> {
    using Type = SelectType<TIndex, TArgs...>;
};
template <typename... TArgs, std::size_t TIndex>
struct TupleElementType<Tuple<TArgs...>&&, TIndex> {
    using Type = SelectType<TIndex, TArgs...>;
};

template <typename TTuple, std::size_t TIndex>
using TupleElement = typename TupleElementType<TTuple, TIndex>::Type;


namespace Detail {
template <typename TFunctor, typename TTuple>
struct ForEachVisitor {
    TTuple&& tuple;
    TFunctor&& functor;

    template <std::size_t TIndex>
    INLINE void visit() {
        functor(tuple.template get<TIndex>());
    }
};

template <typename TFunctor, typename TTuple, template <class> class TTrait>
struct ForEachIfVisitor {
    TTuple&& tuple;
    TFunctor&& functor;

    template <std::size_t TIndex>
    INLINE std::enable_if_t<TTrait<TupleElement<TTuple, TIndex>>::value> visit() {
        functor(tuple.template get<TIndex>());
    }

    template <std::size_t TIndex>
    INLINE std::enable_if_t<!TTrait<TupleElement<TTuple, TIndex>>::value> visit() {}
};
} // namespace Detail

/// For loop to iterate over tuple. The functor must be a generic lambda or have overloaded operators()
/// for all types stored within tuple.
template <typename TFunctor, typename... TArgs>
INLINE void forEach(Tuple<TArgs...>& tuple, TFunctor&& functor) {
    Detail::ForEachVisitor<TFunctor, decltype(tuple)> visitor{ tuple, std::forward<TFunctor>(functor) };
    staticFor<0, sizeof...(TArgs) - 1>(std::move(visitor));
}

/// For loop to iterate over tuple, const version.
template <typename TFunctor, typename... TArgs>
INLINE void forEach(const Tuple<TArgs...>& tuple, TFunctor&& functor) {
    Detail::ForEachVisitor<TFunctor, decltype(tuple)> visitor{ tuple, std::forward<TFunctor>(functor) };
    staticFor<0, sizeof...(TArgs) - 1>(std::move(visitor));
}

/// For loop to iterate over r-value reference of tuple. Values are moved into the functor.
template <typename TFunctor, typename... TArgs>
INLINE void forEach(Tuple<TArgs...>&& tuple, TFunctor&& functor) {
    Detail::ForEachVisitor<TFunctor, decltype(tuple)> visitor{ std::move(tuple),
        std::forward<TFunctor>(functor) };
    staticFor<0, sizeof...(TArgs) - 1>(std::move(visitor));
}

/// Iterates over elements of the tuple and executes a functor if given type traits has value == true
template <template <class T> class TTrait, typename TFunctor, typename... TArgs>
INLINE void forEachIf(Tuple<TArgs...>& tuple, TFunctor&& functor) {
    Detail::ForEachIfVisitor<TFunctor, decltype(tuple), TTrait> visitor{ tuple,
        std::forward<TFunctor>(functor) };
    staticFor<0, sizeof...(TArgs) - 1>(std::move(visitor));
}

/// Iterates over elements of the tuple and executes a functor if given type traits has value == true, const
/// version.
template <template <class T> class TTrait, typename TFunctor, typename... TArgs>
INLINE void forEachIf(const Tuple<TArgs...>& tuple, TFunctor&& functor) {
    Detail::ForEachIfVisitor<TFunctor, decltype(tuple), TTrait> visitor{ tuple,
        std::forward<TFunctor>(functor) };
    staticFor<0, sizeof...(TArgs) - 1>(std::move(visitor));
}


namespace Detail {
template <typename TFunctor, typename TTuple, std::size_t... TIndices>
INLINE decltype(auto) applyImpl(TTuple&& tuple, TFunctor&& functor, std::index_sequence<TIndices...>) {
    return functor(std::forward<TTuple>(tuple).template get<TIndices>()...);
}
} // namespace Detail

/// Expands arguments stored in tuple into parameter pack of a functor.
template <typename TFunctor, typename... TArgs>
INLINE decltype(auto) apply(Tuple<TArgs...>& tuple, TFunctor&& functor) {
    return Detail::applyImpl(tuple, std::forward<TFunctor>(functor), std::index_sequence_for<TArgs...>());
}

/// Expands arguments stored in tuple into parameter pack of a functor, const version.
template <typename TFunctor, typename... TArgs>
INLINE decltype(auto) apply(const Tuple<TArgs...>& tuple, TFunctor&& functor) {
    return Detail::applyImpl(tuple, std::forward<TFunctor>(functor), std::index_sequence_for<TArgs...>());
}

/// Expands arguments stored in r-value reference of tuple into parameter pack of a functor.
template <typename TFunctor, typename... TArgs>
INLINE decltype(auto) apply(Tuple<TArgs...>&& tuple, TFunctor&& functor) {
    return Detail::applyImpl(
        std::move(tuple), std::forward<TFunctor>(functor), std::index_sequence_for<TArgs...>());
}


NAMESPACE_SPH_END
