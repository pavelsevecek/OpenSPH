#pragma once

/// Object capable of storing values of different time, having only one value (and one type) in any moment.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Traits.h"
#include "math/Math.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN


/// Executes a functor with the current value (and current type) as a parameter
template <int n, typename T0, typename... TArgs>
struct ForCurrentType {
    template <typename TFunctor>
    static void action(const int idx, const char* data, TFunctor&& functor) {
        if (idx == n) {
            functor(*reinterpret_cast<const T0*>(data));
        } else {
            ForCurrentType<n + 1, TArgs...>::action(idx, data, std::forward<TFunctor>(functor));
        }
    }

    template <typename TFunctor>
    static void action(const int idx, char* data, TFunctor&& functor) {
        if (idx == n) {
            functor(*reinterpret_cast<T0*>(data));
        } else {
            ForCurrentType<n + 1, TArgs...>::action(idx, data, std::forward<TFunctor>(functor));
        }
    }
};
template <int n, typename T0>
struct ForCurrentType<n, T0> {
    template <typename TFunctor>
    static void action(const int idx, const char* data, TFunctor&& functor) {
        if (idx == n) {
            functor(*reinterpret_cast<const T0*>(data));
        }
    }

    template <typename TFunctor>
    static void action(const int idx, char* data, TFunctor&& functor) {
        if (idx == n) {
            functor(*reinterpret_cast<T0*>(data));
        }
    }
};

template <typename... TArgs>
class Union;

template <typename T0, typename T1, typename... TArgs>
class Union<T0, T1, TArgs...> : public Object {
private:
    union {
        T0 data;
        Union<T1, TArgs...> rest;
    };

public:
    Union() {}

    template <typename T>
    Union(const T& t)
        : rest(t) {}

    Union(const T0& t)
        : data(t) {}

    ~Union() {}

    void destroy(const int idx) {
        if (idx == 0) {
            data.~T0();
        } else {
            rest.destroy(idx - 1);
        }
    }

    template <typename T>
    void copy(const int idx, const T& t) {
        if (idx == 0) {
            data = t.operator const T0&();
        } else {
            rest.copy(idx - 1, t);
        }
    }

    template <typename T>
    Union& operator=(const T& t) {
        rest = t;
        return *this;
    }

    Union& operator=(const T0& t) {
        data = t;
        return *this;
    }

    operator T0&() { return data; }

    template <typename T>
    operator T&() {
        return static_cast<T&>(rest);
    }

    operator const T0&() const { return data; }

    template <typename T>
    operator const T&() const {
        return static_cast<const T&>(rest);
    }
};

/// Specialization for the last type
template <typename T0>
class Union<T0> : public Object {
private:
    T0 data;

public:
    Union() = default;

    Union(const T0& t)
        : data(t) {}

    Union& operator=(const T0& t) {
        data = t;
        return *this;
    }

    void destroy(const int idx) {
        ASSERT(idx == 0);
        data.~T0();
    }

    void copy(const int idx, const T0& t) {
        ASSERT(idx == 0);
        data = t;
    }

    operator T0&() { return data; }

    operator const T0&() const { return data; }
};


/// Variant, an implementation of type-safe union
template <typename... TArgs>
class Variant : public Object {
private:
    Union<TArgs...> impl;
    int typeIdx = -1;

public:
    Variant() = default;

    ~Variant() {
        if (typeIdx != -1) {
            impl.destroy(typeIdx);
        }
    }

    /// Construct variant from value of stored type
    template <typename T>
    Variant(const T& value)
        : impl(value) {
        typeIdx = getTypeIndex<T, TArgs...>;
    }

    Variant(const Variant& other) {
        impl.copy(other.typeIdx, other.impl);
        typeIdx = other.typeIdx;
    }

    /// Universal copy/move operator with type of rhs being one of stored types
    template <typename T>
    Variant& operator=(const T& t) {
        impl    = t;
        typeIdx = getTypeIndex<T, TArgs...>;
        return *this;
    }

    Variant& operator=(const Variant& other) {
        impl.copy(other.typeIdx, other.impl);
        typeIdx = other.typeIdx;
        return *this;
    }

    int getTypeIdx() const { return typeIdx; }

    /// Implicit conversion to one of stored values. Performs a compile-time check that the type is contained
    /// in Variant, and runtime check that the variant currently holds value of given type.
    template <typename T>
    operator T&() {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        ASSERT((typeIdx == getTypeIndex<T, TArgs...>));
        return static_cast<T&>(impl);
    }

    /// const version
    template <typename T>
    operator T() const {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        ASSERT((typeIdx == getTypeIndex<T, TArgs...>));
        return static_cast<const T&>(impl);
    }


    /// Returns the stored value in the variant. Safer than implicit conversion as it returns NOTHING in case
    /// the value is currently not stored in variant.
    template <typename T>
    Optional<T> get() const {
        constexpr int idx = getTypeIndex<T, TArgs...>;
        static_assert(idx != -1, "Cannot convert variant to this type");
        if (typeIdx != getTypeIndex<T, TArgs...>) {
            return NOTHING;
        }
        return impl.operator const T&();
    }
};

NAMESPACE_SPH_END
