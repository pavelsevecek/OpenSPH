#pragma once

/// Re-implementation of std::tuple with some additional functionality.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Traits.h"

NAMESPACE_SPH_BEGIN


/// Data storage of tuple of elements
template <typename... TArgs>
class TupleImpl;

template <typename T0, typename... TArgs>
class TupleImpl<T0, TArgs...> : public TupleImpl<TArgs...> {
public:
    T0 elem;

    constexpr TupleImpl() = default;

    template<typename T, typename... Ts>
    explicit TupleImpl(T&& first, Ts&&... rest)
        : TupleImpl<TArgs...>(std::forward<Ts>(rest)...)
        , elem(std::forward<T>(first)) {}

    /// Move operator from a tuple (of generally different type)
    template <typename T, typename... TS>
    TupleImpl& operator=(TupleImpl<T, TS...>&& other) {
        elem                                     = std::move(other.elem);
        *static_cast<TupleImpl<TArgs...>*>(this) = std::move(*static_cast<TupleImpl<TS...>*>(&other));
        return *this;
    }

    /// Copy operator
    template <typename T, typename... TS>
    TupleImpl& operator=(const TupleImpl<T, TS...>& other) {
        elem                                     = other.elem;
        *static_cast<TupleImpl<TArgs...>*>(this) = *static_cast<TupleImpl<TS...>*>(&other);
        return *this;
    }
};

/// The specialization ending the recursive implementation
template <typename T0>
class TupleImpl<T0> : public Object {
public:
    T0 elem;

    constexpr TupleImpl() = default;

    template<typename T>
    explicit constexpr TupleImpl(T&& last)
        : elem(std::forward<T>(last)) {}

    template <typename T>
    TupleImpl& operator=(TupleImpl<T>&& other) {
        elem = std::move(other.elem);
        return *this;
    }

    template <typename T>
    TupleImpl& operator=(const TupleImpl<T>& other) {
        elem = other.elem;
        return *this;
    }
};

/// A helper type extracting the type of the n-th element
template <int I, int N, typename T0, typename... TArgs>
class GetTupleImplType : public GetTupleImplType<I + 1, N, TArgs...> {};

template <int N, typename T0, typename... TArgs>
class GetTupleImplType<N, N, T0, TArgs...> : public Object {
public:
    using Type = TupleImpl<T0, TArgs...>;
};

template <int I, int N, typename... TArgs>
using GetTupleImpl = typename GetTupleImplType<I, N, TArgs...>::Type;


/// The main class for tuple
template <typename... TArgs>
class Tuple : public TupleImpl<TArgs...> {
protected:
    template <int n>
    using Data = GetTupleImpl<0, n, TArgs...>;

public:
    template <int n>
    using Type = std::decay_t<SelectType<n, TArgs...>>;

    static constexpr int size = sizeof...(TArgs);

    Tuple() = default;

    constexpr Tuple(const Tuple&) = default;

    constexpr Tuple(Tuple&&) = default;

    template <typename T0, typename... TS>
    explicit constexpr Tuple(T0&& first, TS&&... rest)
        : TupleImpl<TArgs...>(std::forward<T0>(first), std::forward<TS>(rest)...) {}

    Tuple& operator=(const Tuple&) = default;

    template <int n>
    constexpr Type<n>& get() {
        return static_cast<Data<n>*>(this)->elem;
    }

    template <int n>
    const Type<n>& get() const {
        return static_cast<const Data<n>*>(this)->elem;
    }

    template <typename... TS>
    Tuple& operator=(const Tuple<TS...>& other) {
        *static_cast<TupleImpl<TArgs...>*>(this) = *static_cast<TupleImpl<TS...>*>(&other);
        return *this;
    }

    template <typename... TS>
    Tuple& operator=(Tuple<TS...>&& other) {
        *static_cast<TupleImpl<TArgs...>*>(this) = std::move(*static_cast<TupleImpl<TS...>*>(&other));
        return *this;
    }
};


template <typename... TArgs>
inline auto makeTuple(TArgs&&... args) {
    return Tuple<std::decay_t<TArgs>...>(std::forward<TArgs>(args)...);
}

template <typename... TArgs>
inline Tuple<TArgs&...> tie(TArgs&... args) {
    return Tuple<TArgs&...>(args...);
}

template <typename... TArgs>
inline Tuple<TArgs&&...> forwardAsTuple(TArgs&&... args) {
    return Tuple<TArgs&&...>(std::forward<TArgs>(args)...);
}


/// placeholder for unused variables in
struct Ignore {
    template <class T>
    const Ignore& operator=(T&&) const {
        return *this;
    }
};

const Ignore IGNORE;

/// for loop to iterate over tuple. The functor must be a generic lambda or have overloaded operators()
/// for all types stored within tuple.
template <int i, int n, typename TFunctor, typename... TArgs>
struct forEachImpl : public Object {
    static void action(Tuple<TArgs...>& tuple, TFunctor functor) {
        functor(tuple.template get<i>());
        forEachImpl<i + 1, n, TFunctor, TArgs...>::action(tuple, functor);
    }

    static void action(const Tuple<TArgs...>& tuple, TFunctor functor) {
        functor(tuple.template get<i>());
        forEachImpl<i + 1, n, TFunctor, TArgs...>::action(tuple, functor);
    }
};

template <int n, typename TFunctor, typename... TArgs>
struct forEachImpl<n, n, TFunctor, TArgs...> : public Object {
    static void action(Tuple<TArgs...>& tuple, TFunctor functor) { functor(tuple.template get<n>()); }

    static void action(const Tuple<TArgs...>& tuple, TFunctor functor) { functor(tuple.template get<n>()); }
};

// non-const reference
template <typename TFunctor, typename... TArgs>
void forEach(Tuple<TArgs...>& tuple, TFunctor functor) {
    forEachImpl<0, sizeof...(TArgs)-1, TFunctor, TArgs...>::action(tuple, functor);
}

// const reference
template <typename TFunctor, typename... TArgs>
void forEach(const Tuple<TArgs...>& tuple, TFunctor functor) {
    forEachImpl<0, sizeof...(TArgs)-1, TFunctor, TArgs...>::action(tuple, functor);
}

/// getter, to avoid using "tuple.template get<0>", ...
template <int n, typename TTuple>
decltype(auto) get(TTuple&& tuple) {
    return std::forward<TTuple>(tuple).template get<n>();
}

/// tuple size
template <typename TTuple>
struct TupleSize;

template <typename... TArgs>
struct TupleSize<Tuple<TArgs...>> {
    static constexpr int value = sizeof...(TArgs);
};


namespace Detail {
    template <int... TIndices>
    struct IndexSequence : public Object {
        static constexpr int size = sizeof...(TIndices);

        using Next = IndexSequence<TIndices..., size>;
    };

    template <int TSize>
    struct MakeIndexSequence {
        using Type = typename MakeIndexSequence<TSize - 1>::Type::Next;
    };

    template <>
    struct MakeIndexSequence<0> {
        using Type = IndexSequence<>;
    };

    template <typename TFunctor, typename TTuple, int... TIndices>
    decltype(auto) applyImpl(TFunctor&& functor, TTuple&& tuple, IndexSequence<TIndices...>) {
        return functor(get<TIndices>(tuple)...);
    }

    template <typename T, typename... TArgs, int... TIndices>
    Tuple<TArgs..., T> appendImpl(const Tuple<TArgs...>& tuple, const T& value, IndexSequence<TIndices...>) {
        return Tuple<TArgs..., T>(tuple.template get<TIndices>()..., value);
    }
}

template <typename TFunctor, typename TTuple>
decltype(auto) apply(TFunctor&& functor, TTuple&& tuple) {
    using Sequence = typename Detail::MakeIndexSequence<TupleSize<std::decay_t<TTuple>>::value>::Type;
    return Detail::applyImpl(std::forward<TFunctor>(functor), std::forward<TTuple>(tuple), Sequence());
}


template <typename TFunctor, typename... TArgs>
inline auto transformTuple(Tuple<TArgs...>& tuple, TFunctor&& functor) {
    return apply([capturedFunctor = std::forward<TFunctor>(functor)](
                     auto&... values) { return tie(capturedFunctor(values)...); },
                 tuple);
}


template<typename T, typename... TArgs>
Tuple<TArgs..., T> append(const Tuple<TArgs...>& tuple, const T& value) {
    using Sequence = typename Detail::MakeIndexSequence<sizeof...(TArgs)>::Type;
    return appendImpl(tuple, value, Sequence());
}

/*template <typename... TArgs>
struct TupleIterator {
    Tuple<Iterator<TArgs>...> iterators;

    TupleIterator(Tuple<Iterator<TArgs>...>&& iterators)
        : iterators(std::move(iterators)) {}

    TupleIterator(const TupleIterator& other)
        : iterators(other.iterators) {}

    TupleIterator& operator++() {
        forEach(iterators, [](auto& iter) { iter++; });
        return *this;
    }
    TupleIterator operator++(int) {
        TupleIterator tmp(*this);
        operator++();
        return tmp;
    }
    bool operator==(const TupleIterator& rhs) { return get<0>(iterators) == get<0>(rhs.iterators); }
    bool operator!=(const TupleIterator& rhs) { return get<0>(iterators) != get<0>(rhs.iterators); }

    auto operator*() {
        return apply([](auto&&... params) { return makeTuple(*std::forward<decltype(params)>(params)...); },
                     iterators);
    }
};
*/


NAMESPACE_SPH_END
