#pragma once

#include "geometry/Vector.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN

/// c++14 sort of template parameters
template <Size... Is>
constexpr auto sortIndices() {
    constexpr Size N = sizeof...(Is);
    ConstexprArray<Size, N> array{ Is... };

    for (Size i = 0; i < N - 1; i++) {
        for (Size j = 0; j < N - i - 1; j++) {
            if (array[j] > array[j + 1]) {
                Size k = array[j];
                array[j] = array[j + 1];
                array[j + 1] = k;
            }
        }
    }
    return array;
}

namespace Detail {
    constexpr Size multipoleComponentCnt(Size order) {
        return (order + 1) * (order + 2) / 2;
    }
    template <Size Last>
    constexpr Size sum() {
        return Last;
    }
    template <Size First, Size Second, Size... Idxs>
    constexpr Size sum() {
        return First + sum<Second, Idxs...>();
    }

    template <Size... Idxs>
    struct MultipoleMappingImpl;

    template <Size Second, Size... Idxs>
    struct MultipoleMappingImpl<X, Second, Idxs...> {
        static constexpr Size value = MultipoleMappingImpl<Second, Idxs...>::value;
    };
    template <Size I>
    struct MultipoleMappingImpl<I> {
        static constexpr Size value = I;
    };
    template <Size Second, Size... Idxs>
    struct MultipoleMappingImpl<Y, Second, Idxs...> {
        static constexpr Size value =
            multipoleComponentCnt(sizeof...(Idxs) + 1) - 1 - sizeof...(Idxs) + sum<Second, Idxs...>();
    };
    template <Size Second, Size... Idxs>
    struct MultipoleMappingImpl<Z, Second, Idxs...> {
        static constexpr Size value = multipoleComponentCnt(sizeof...(Idxs) + 2) - 1;
    };

    template <std::size_t... Idxs, std::size_t... Is>
    constexpr Size expandMultipoleArray(std::index_sequence<Is...>) {
        constexpr ConstexprArray<Size, sizeof...(Idxs)> ar = sortIndices<Idxs...>();
        return MultipoleMappingImpl<ar[Is]...>::value;
    }

    template <Size... Idxs>
    struct MultipoleMapping {
        using Sequence = std::make_index_sequence<sizeof...(Idxs)>;

        static constexpr Size value = expandMultipoleArray<Idxs...>(Sequence{});
    };

    template <Size I>
    struct MultipoleMapping<I> {
        static constexpr Size value = I;
    };
    template <>
    struct MultipoleMapping<> {
        static constexpr Size value = 0;
    };
}

template <Size Order>
class Multipole {
private:
    static constexpr Size COMPONENT_CNT = Detail::multipoleComponentCnt(Order);

    Float data[COMPONENT_CNT];

public:
    static constexpr Size ORDER = Order;

    Multipole() = default;

    Multipole(const Float f) {
        for (Float& v : data) {
            v = f;
        }
    }

    template <Size... Idxs>
    INLINE Float value() const {
        static_assert(sizeof...(Idxs) == Order, "Number of indices must match the order");
        const Size idx = Detail::MultipoleMapping<Idxs...>::value;
        ASSERT(idx < COMPONENT_CNT, idx);
        return *(&data[0] + idx);
    }

    template <Size... Idxs>
    INLINE Float& value() {
        static_assert(sizeof...(Idxs) == Order, "Number of indices must match the order");
        const Size idx = Detail::MultipoleMapping<Idxs...>::value;
        ASSERT(idx < COMPONENT_CNT, idx);
        return *(&data[0] + idx);
    }

    INLINE Float operator[](const Size idx) const {
        ASSERT(idx < COMPONENT_CNT, idx);
        return data[idx];
    }

    Multipole& operator+=(const Multipole& other) {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            data[i] += other[i];
        }
        return *this;
    }

    Multipole operator*(const Float f) const {
        Multipole m;
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            m.data[i] = data[i] * f;
        }
        return m;
    }

    bool operator==(const Multipole& other) const {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            if (data[i] != other[i]) {
                return false;
            }
        }
        return true;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Multipole& m) {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            stream << m.data[i] << " ";
        }
        return stream;
    }
};

template <Size N>
INLINE auto almostEqual(const Multipole<N>& f1, const Multipole<N>& f2, const Float& eps = EPS) {
    for (Size i = 0; i < Detail::multipoleComponentCnt(N); ++i) {
        if (!almostEqual(f1[i], f2[i], eps)) {
            return false;
        }
    }
    return true;
}


/// Creates multipole by evaluating given object for each independent component
template <Size N>
struct MakeMultipole;

template <>
struct MakeMultipole<0> {
    template <typename TValue>
    static Multipole<0> make(const TValue& v) {
        Multipole<0> m;
        m.value() = v.value();
        return m;
    }
};

template <>
struct MakeMultipole<1> {
    template <typename TValue>
    static Multipole<1> make(const TValue& v) {
        Multipole<1> m;
        m.value<X>() = v.template value<X>();
        m.value<Y>() = v.template value<Y>();
        m.value<Z>() = v.template value<Z>();
        return m;
    }
};

template <>
struct MakeMultipole<2> {
    template <typename TValue>
    static Multipole<2> make(const TValue& v) {
        Multipole<2> m;
        m.value<X, X>() = v.template value<X, X>();
        m.value<X, Y>() = v.template value<X, Y>();
        m.value<X, Z>() = v.template value<X, Z>();

        m.value<Y, Y>() = v.template value<Y, Y>();
        m.value<Y, Z>() = v.template value<Y, Z>();

        m.value<Z, Z>() = v.template value<Z, Z>();
        return m;
    }
};

template <>
struct MakeMultipole<3> {
    template <typename TValue>
    static Multipole<3> make(const TValue& v) {
        Multipole<3> m;
        m.value<X, X, X>() = v.template value<X, X, X>();
        m.value<X, X, Y>() = v.template value<X, X, Y>();
        m.value<X, X, Z>() = v.template value<X, X, Z>();
        m.value<X, Y, Y>() = v.template value<X, Y, Y>();
        m.value<X, Y, Z>() = v.template value<X, Y, Z>();
        m.value<X, Z, Z>() = v.template value<X, Z, Z>();

        m.value<Y, Y, Y>() = v.template value<Y, Y, Y>();
        m.value<Y, Y, Z>() = v.template value<Y, Y, Z>();
        m.value<Y, Z, Z>() = v.template value<Y, Z, Z>();

        m.value<Z, Z, Z>() = v.template value<Z, Z, Z>();
        return m;
    }
};

template <>
struct MakeMultipole<4> {
    template <typename TValue>
    static Multipole<4> make(const TValue& v) {
        Multipole<4> m;
        m.value<X, X, X, X>() = v.template value<X, X, X, X>();
        m.value<X, X, X, Y>() = v.template value<X, X, X, Y>();
        m.value<X, X, X, Z>() = v.template value<X, X, X, Z>();
        m.value<X, X, Y, Y>() = v.template value<X, X, Y, Y>();
        m.value<X, X, Y, Z>() = v.template value<X, X, Y, Z>();
        m.value<X, X, Z, Z>() = v.template value<X, X, Z, Z>();
        m.value<X, Y, Y, Y>() = v.template value<X, Y, Y, Y>();
        m.value<X, Y, Y, Z>() = v.template value<X, Y, Y, Z>();
        m.value<X, Y, Z, Z>() = v.template value<X, Y, Z, Z>();
        m.value<X, Z, Z, Z>() = v.template value<X, Z, Z, Z>();

        m.value<Y, Y, Y, Y>() = v.template value<Y, Y, Y, Y>();
        m.value<Y, Y, Y, Z>() = v.template value<Y, Y, Y, Z>();
        m.value<Y, Y, Z, Z>() = v.template value<Y, Y, Z, Z>();
        m.value<Y, Z, Z, Z>() = v.template value<Y, Z, Z, Z>();

        m.value<Z, Z, Z, Z>() = v.template value<Z, Z, Z, Z>();
        return m;
    }
};

template <Size N, typename TValue>
Multipole<N> makeMultipole(const TValue& v) {
    return MakeMultipole<N>::make(v);
}


namespace Detail {
    constexpr Size tracelessMultipoleComponentCnt(Size order) {
        return 2 * order + 1;
    }

    template <Size... Idxs>
    struct TracelessMultipoleMappingImpl;

    template <Size Second, Size... Idxs>
    struct TracelessMultipoleMappingImpl<X, Second, Idxs...> {
        static constexpr Size value = MultipoleMapping<Second, Idxs...>::value;
    };
    template <Size Second, Size... Idxs>
    struct TracelessMultipoleMappingImpl<Y, Second, Idxs...> {
        static constexpr Size value = tracelessMultipoleComponentCnt(sizeof...(Idxs) + 1) - 1 -
                                      sizeof...(Idxs) + sum<Second, Idxs...>();
    };
    template <Size I>
    struct TracelessMultipoleMappingImpl<I> {
        static constexpr Size value = I;
    };

    template <std::size_t... Idxs, std::size_t... Is>
    constexpr Size expandTracelessMultipoleArray(std::index_sequence<Is...>) {
        constexpr ConstexprArray<Size, sizeof...(Idxs)> ar = sortIndices<Idxs...>();
        return TracelessMultipoleMappingImpl<ar[Is]...>::value;
    }

    template <Size... Idxs>
    struct TracelessMultipoleMapping {
        using Sequence = std::make_index_sequence<sizeof...(Idxs)>;

        static constexpr Size value = expandTracelessMultipoleArray<Idxs...>(Sequence{});
    };
    template <Size I>
    struct TracelessMultipoleMapping<I> {
        static constexpr Size value = I;
    };
}

template <Size... Idxs>
struct TracelessMultipoleComponent;

template <Size Order>
class TracelessMultipole {
private:
    static constexpr Size COMPONENT_CNT = Detail::tracelessMultipoleComponentCnt(Order);

    Float data[COMPONENT_CNT];

public:
    static constexpr Size ORDER = Order;

    TracelessMultipole() = default;

    TracelessMultipole(const Float f) {
        for (Float& v : data) {
            v = f;
        }
    }

    template <Size... Idxs>
    INLINE Float value() const {
        return TracelessMultipoleComponent<Idxs...>{}.get(*this);
    }

    template <Size... Idxs>
    INLINE Float& value() {
        static_assert(sizeof...(Idxs) == Order, "Number of indices must match the order");
        const Size idx = Detail::TracelessMultipoleMapping<Idxs...>::value;
        ASSERT(idx < COMPONENT_CNT, idx);
        return *(&data[0] + idx);
    }

    template <Size... Idxs>
    INLINE Float valueImpl() const {
        static_assert(sizeof...(Idxs) == Order, "Number of indices must match the order");
        const Size idx = Detail::TracelessMultipoleMapping<Idxs...>::value;
        ASSERT(idx < COMPONENT_CNT, idx);
        return *(&data[0] + idx);
    }

    INLINE Float operator[](const Size idx) const {
        ASSERT(idx < COMPONENT_CNT, idx);
        return data[idx];
    }

    TracelessMultipole& operator+=(const TracelessMultipole& other) {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            data[i] += other.data[i];
        }
        return *this;
    }

    bool operator==(const TracelessMultipole& other) const {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            if (data[i] != other.data[i]) {
                return false;
            }
        }
        return true;
    }

    friend std::ostream& operator<<(std::ostream& stream, const TracelessMultipole& m) {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            stream << m.data[i] << " ";
        }
        return stream;
    }
};

template <>
class TracelessMultipole<1> {
private:
    Vector data;

public:
    TracelessMultipole() = default;

    TracelessMultipole(const Float v)
        : data(v) {}

    static constexpr Size ORDER = 1;

    template <Size Idx>
    INLINE Float& value() {
        return data[Idx];
    }

    template <Size Idx>
    INLINE Float value() const {
        return data[Idx];
    }

    INLINE Float operator[](const Size idx) const {
        ASSERT(idx < 3, idx);
        return data[idx];
    }

    Vector vector() const {
        return data;
    }

    TracelessMultipole& operator+=(const TracelessMultipole& other) {
        data += other.data;
        return *this;
    }

    bool operator==(const TracelessMultipole& other) const {
        return data == other.data;
    }

    friend std::ostream& operator<<(std::ostream& stream, const TracelessMultipole& m) {
        stream << m.data;
        return stream;
    }
};


template <>
class TracelessMultipole<0> {
private:
    Float data;

public:
    TracelessMultipole() = default;

    TracelessMultipole(const Float v)
        : data(v) {}

    static constexpr Size ORDER = 0;

    INLINE Float& value() {
        return data;
    }

    INLINE Float value() const {
        return data;
    }

    INLINE Float operator[](const Size UNUSED_IN_RELEASE(idx)) const {
        ASSERT(idx == 0);
        return data;
    }

    INLINE operator Float() const {
        return data;
    }

    TracelessMultipole& operator+=(const TracelessMultipole& other) {
        data += other.data;
        return *this;
    }

    bool operator==(const TracelessMultipole& other) const {
        return data == other.data;
    }
};


template <Size N>
INLINE auto almostEqual(const TracelessMultipole<N>& f1,
    const TracelessMultipole<N>& f2,
    const Float& eps = EPS) {
    for (Size i = 0; i < Detail::tracelessMultipoleComponentCnt(N); ++i) {
        if (!almostEqual(f1[i], f2[i], eps)) {
            return false;
        }
    }
    return true;
}

/// Creates multipole by evaluating given object for each independent component

template <Size N>
struct MakeTracelessMultipole;

template <>
struct MakeTracelessMultipole<0> {
    template <typename TValue>
    static TracelessMultipole<0> make(const TValue& v) {
        TracelessMultipole<0> m;
        m.value() = v.value();
        return m;
    }
};

template <>
struct MakeTracelessMultipole<1> {
    template <typename TValue>
    static TracelessMultipole<1> make(const TValue& v) {
        TracelessMultipole<1> m;
        m.value<X>() = v.template value<X>();
        m.value<Y>() = v.template value<Y>();
        m.value<Z>() = v.template value<Z>();
        return m;
    }
};

template <>
struct MakeTracelessMultipole<2> {
    template <typename TValue>
    static TracelessMultipole<2> make(const TValue& v) {
        TracelessMultipole<2> m;
        m.value<X, X>() = v.template value<X, X>();
        m.value<X, Y>() = v.template value<X, Y>();
        m.value<X, Z>() = v.template value<X, Z>();

        m.value<Y, Y>() = v.template value<Y, Y>();
        m.value<Y, Z>() = v.template value<Y, Z>();
        return m;
    }
};

template <>
struct MakeTracelessMultipole<3> {
    template <typename TValue>
    static TracelessMultipole<3> make(const TValue& v) {
        TracelessMultipole<3> m;
        m.value<X, X, X>() = v.template value<X, X, X>();
        m.value<X, X, Y>() = v.template value<X, X, Y>();
        m.value<X, X, Z>() = v.template value<X, X, Z>();
        m.value<X, Y, Y>() = v.template value<X, Y, Y>();
        m.value<X, Y, Z>() = v.template value<X, Y, Z>();

        m.value<Y, Y, Y>() = v.template value<Y, Y, Y>();
        m.value<Y, Y, Z>() = v.template value<Y, Y, Z>();
        return m;
    }
};

template <>
struct MakeTracelessMultipole<4> {
    template <typename TValue>
    static TracelessMultipole<4> make(const TValue& v) {
        TracelessMultipole<4> m;
        m.value<X, X, X, X>() = v.template value<X, X, X, X>();
        m.value<X, X, X, Y>() = v.template value<X, X, X, Y>();
        m.value<X, X, X, Z>() = v.template value<X, X, X, Z>();
        m.value<X, X, Y, Y>() = v.template value<X, X, Y, Y>();
        m.value<X, X, Y, Z>() = v.template value<X, X, Y, Z>();
        m.value<X, Y, Y, Y>() = v.template value<X, Y, Y, Y>();
        m.value<X, Y, Y, Z>() = v.template value<X, Y, Y, Z>();

        m.value<Y, Y, Y, Y>() = v.template value<Y, Y, Y, Y>();
        m.value<Y, Y, Y, Z>() = v.template value<Y, Y, Y, Z>();
        return m;
    }
};

template <Size N, typename TValue>
TracelessMultipole<N> makeTracelessMultipole(const TValue& v) {
    return MakeTracelessMultipole<N>::make(v);
}

// computes component from traceless multipole
template <Size... Idxs>
struct TracelessMultipoleComponent {
    INLINE static Float get(const TracelessMultipole<sizeof...(Idxs)>& m) {
        return m.template valueImpl<Idxs...>();
    }
};
template <>
struct TracelessMultipoleComponent<Z, Z> {
    INLINE static Float get(const TracelessMultipole<2>& m) {
        return -m.valueImpl<X, X>() - m.valueImpl<Y, Y>();
    }
};
template <Size I>
struct TracelessMultipoleComponent<I, Z, Z> {
    INLINE static Float get(const TracelessMultipole<3>& m) {
        return -m.valueImpl<X, X, I>() - m.valueImpl<Y, Y, I>();
    }
};
template <Size I, Size J>
struct TracelessMultipoleComponent<I, J, Z, Z> {
    INLINE static Float get(const TracelessMultipole<4>& m) {
        return -m.valueImpl<X, X, I, J>() - m.valueImpl<Y, Y, I, J>();
    }
};


INLINE constexpr Size factorial(const Size n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
static_assert(factorial(1) == 1, "static test failed");
static_assert(factorial(2) == 2, "static test failed");
static_assert(factorial(3) == 6, "static test failed");
static_assert(factorial(4) == 24, "static test failed");

INLINE constexpr Size doubleFactorial(const Size n) {
    return n <= 1 ? 1 : n * doubleFactorial(n - 2);
}
static_assert(doubleFactorial(1) == 1, "static test failed");
static_assert(doubleFactorial(2) == 2, "static test failed");
static_assert(doubleFactorial(3) == 3, "static test failed");
static_assert(doubleFactorial(4) == 8, "static test failed");
static_assert(doubleFactorial(5) == 15, "static test failed");


namespace MomentOperators {

    template <Size Order>
    struct Delta {
        static constexpr Size ORDER = Order;

        template <Size I, Size J, Size K, Size L, Size... Rest>
        INLINE static constexpr Float value() {
            static_assert(sizeof...(Rest) + 4 == Order, "Invalid number of indices");
            return ((I == J) ? 1 : 0) * Delta<Order - 2>::template value<K, L, Rest...>();
        }
        template <Size I, Size J>
        INLINE static constexpr Float value() {
            static_assert(Order == 2, "Invalid number of indices");
            return ((I == J) ? 1 : 0);
        }
    };


    template <Size N1, Size N2, typename Value1, typename Value2>
    struct Permutations;

    template <Size N, typename Value1, typename Value2>
    struct Permutations<N, 0, Value1, Value2> {
        const Value1& v1;
        const Value2& v2;
        static constexpr Size ORDER = N;

        template <Size... Is>
        INLINE constexpr Float value() const {
            static_assert(sizeof...(Is) == N, "Invalid number of indices");
            return v1.template value<Is...>() * v2.value();
        }
    };
    template <Size N, typename Value1, typename Value2>
    struct Permutations<0, N, Value1, Value2> {
        const Value1& v1;
        const Value2& v2;
        static constexpr Size ORDER = N;

        template <Size... Is>
        INLINE constexpr Float value() const {
            static_assert(sizeof...(Is) == N, "Invalid number of indices");
            return v1.value() * v2.template value<Is...>();
        }
    };
    template <typename Value1, typename Value2>
    struct Permutations<2, 2, Value1, Value2> {
        const Value1& v1;
        const Value2& v2;
        static constexpr Size ORDER = 4;

        template <Size I, Size J, Size K, Size L>
        INLINE constexpr Float value() const {
            return v1.template value<I, J>() * v2.template value<K, L>() +
                   v1.template value<I, K>() * v2.template value<J, L>() +
                   v1.template value<I, L>() * v2.template value<J, K>() +
                   v1.template value<J, K>() * v2.template value<I, L>() +
                   v1.template value<J, L>() * v2.template value<I, K>() +
                   v1.template value<K, L>() * v2.template value<I, J>();
        }
    };
    template <typename Value1, typename Value2>
    struct Permutations<3, 1, Value1, Value2> {
        const Value1& v1;
        const Value2& v2;
        static constexpr Size ORDER = 4;

        template <Size I, Size J, Size K, Size L>
        INLINE constexpr Float value() const {
            return v1.template value<I, J, K>() * v2.template value<L>() +
                   v1.template value<I, J, L>() * v2.template value<K>() +
                   v1.template value<I, K, L>() * v2.template value<J>() +
                   v1.template value<J, K, L>() * v2.template value<I>();
        }
    };
    template <typename Value1, typename Value2>
    struct Permutations<2, 1, Value1, Value2> {
        const Value1& v1;
        const Value2& v2;
        static constexpr Size ORDER = 3;

        template <Size I, Size J, Size K>
        INLINE constexpr Float value() const {
            return v1.template value<I, J>() * v2.template value<K>() +
                   v1.template value<J, K>() * v2.template value<I>() +
                   v1.template value<K, I>() * v2.template value<J>();
        }
    };
    template <typename Value1, typename Value2>
    struct Permutations<1, 2, Value1, Value2> {
        const Value1& v1;
        const Value2& v2;
        static constexpr Size ORDER = 3;

        template <Size I, Size J, Size K>
        INLINE constexpr Float value() const {
            return v1.template value<I>() * v2.template value<J, K>() +
                   v1.template value<J>() * v2.template value<K, I>() +
                   v1.template value<K>() * v2.template value<I, J>();
        }
    };

    template <typename Value1, typename Value2>
    Permutations<Value1::ORDER, Value2::ORDER, Value1, Value2> makePermutations(const Value1& v1,
        const Value2& v2) {
        return Permutations<Value1::ORDER, Value2::ORDER, Value1, Value2>{ v1, v2 };
    }

    template <typename TValue>
    struct Contraction {
        const TValue& v;

        template <Size... Is>
        INLINE constexpr Float value() const {
            return v.template value<0, 0, Is...>() + v.template value<1, 1, Is...>() +
                   v.template value<2, 2, Is...>();
        }
    };

    template <typename TValue>
    Contraction<TValue> makeContraction(const TValue& v) {
        return Contraction<TValue>{ v };
    }

    template <Size N, Size O1, typename TValue1, typename TValue2>
    struct InnerProduct;

    template <typename TValue1, typename TValue2>
    struct InnerProduct<1, 2, TValue1, TValue2> {
        const TValue1& v1;
        const TValue2& v2;

        template <Size I, Size... Is>
        INLINE constexpr Float value() const {
            static_assert(TValue1::ORDER == 2, "Invalid number of indices");
            static_assert(TValue2::ORDER == sizeof...(Is) + 1, "Invalid number of indices");
            return v1.template value<0, I>() * v2.template value<0, Is...>() +
                   v1.template value<1, I>() * v2.template value<1, Is...>() +
                   v1.template value<2, I>() * v2.template value<2, Is...>();
        }
    };
    template <typename TValue1, typename TValue2>
    struct InnerProduct<1, 1, TValue1, TValue2> {
        const TValue1& v1;
        const TValue2& v2;

        template <Size... Is>
        INLINE constexpr Float value() const {
            static_assert(TValue1::ORDER == 1, "Invalid number of indices");
            static_assert(TValue2::ORDER == sizeof...(Is) + 1, "Invalid number of indices");
            return v1.template value<0>() * v2.template value<0, Is...>() +
                   v1.template value<1>() * v2.template value<1, Is...>() +
                   v1.template value<2>() * v2.template value<2, Is...>();
        }
    };

    template <typename TValue1, typename TValue2>
    struct InnerProduct<2, 2, TValue1, TValue2> {
        const TValue1& v1;
        const TValue2& v2;

        template <Size... Is>
        INLINE constexpr Float value() const {
            static_assert(TValue1::ORDER == 2, "Invalid number of indices");
            static_assert(TValue2::ORDER == sizeof...(Is) + 2, "Invalid number of indices");
            return v1.template value<0, 0>() * v2.template value<0, 0, Is...>() +
                   v1.template value<0, 1>() * v2.template value<0, 1, Is...>() +
                   v1.template value<0, 2>() * v2.template value<0, 2, Is...>() +
                   v1.template value<1, 0>() * v2.template value<1, 0, Is...>() +
                   v1.template value<1, 1>() * v2.template value<1, 1, Is...>() +
                   v1.template value<1, 2>() * v2.template value<1, 2, Is...>() +
                   v1.template value<2, 0>() * v2.template value<2, 0, Is...>() +
                   v1.template value<2, 1>() * v2.template value<2, 1, Is...>() +
                   v1.template value<2, 2>() * v2.template value<2, 2, Is...>();
        }
    };

    template <Size N, typename TValue1, typename TValue2>
    InnerProduct<N, TValue1::ORDER, TValue1, TValue2> makeInner(const TValue1& v1, const TValue2& v2) {
        return InnerProduct<N, TValue1::ORDER, TValue1, TValue2>{ v1, v2 };
    }


    template <typename TValue>
    struct MultiplyByScalar {
        const TValue& v;
        const Float f;

        template <Size... Is>
        INLINE constexpr Float value() const {
            return f * v.template value<Is...>();
        }
    };

    template <Size O1, typename TValue1, typename TValue2>
    struct MultiplyTwo;

    template <typename TValue1, typename TValue2>
    struct MultiplyTwo<1, TValue1, TValue2> {
        const TValue1& v1;
        const TValue2& v2;

        template <Size I, Size... Is>
        INLINE constexpr Float value() const {
            static_assert(
                TValue1::ORDER == 1 && TValue2::ORDER == sizeof...(Is), "Invalid number of indices");
            return v1.template value<I>() * v2.template value<Is...>();
        }
    };

    template <typename TValue1, typename TValue2>
    struct MultiplyTwo<2, TValue1, TValue2> {
        const TValue1& v1;
        const TValue2& v2;

        template <Size I, Size J, Size... Is>
        INLINE constexpr Float value() const {
            static_assert(
                TValue1::ORDER == 2 && TValue2::ORDER == sizeof...(Is), "Invalid number of indices");
            return v1.template value<I, J>() * v2.template value<Is...>();
        }
    };

    template <typename TValue1, typename TValue2>
    struct MultiplyTwo<4, TValue1, TValue2> {
        const TValue1& v1;
        const TValue2& v2;

        template <Size I, Size J, Size K, Size L, Size... Is>
        INLINE constexpr Float value() const {
            static_assert(
                TValue1::ORDER == 4 && TValue2::ORDER == sizeof...(Is), "Invalid number of indices");
            return v1.template value<I, J, K, L>() * v2.template value<Is...>();
        }
    };

    template <typename TValue>
    MultiplyByScalar<TValue> operator*(const TValue& v, const Float f) {
        return MultiplyByScalar<TValue>{ v, f };
    }

    template <typename TValue1, typename TValue2>
    MultiplyTwo<TValue1::ORDER, TValue1, TValue2> makeMultiply(const TValue1& v1, const TValue2& v2) {
        return MultiplyTwo<TValue1::ORDER, TValue1, TValue2>{ v1, v2 };
    }

    template <typename TValue1, typename TValue2>
    struct Sum {
        const TValue1& v1;
        const TValue2& v2;

        template <Size... Is>
        INLINE constexpr Float value() const {
            return v1.template value<Is...>() + v2.template value<Is...>();
        }
    };

    template <typename TValue1, typename TValue2>
    Sum<TValue1, TValue2> operator+(const TValue1& v1, const TValue2& v2) {
        return Sum<TValue1, TValue2>{ v1, v2 };
    }

    template <typename TValue1, typename TValue2>
    struct Difference {
        const TValue1& v1;
        const TValue2& v2;

        template <Size... Is>
        INLINE constexpr Float value() const {
            return v1.template value<Is...>() - v2.template value<Is...>();
        }
    };

    template <typename TValue1, typename TValue2>
    Difference<TValue1, TValue2> operator-(const TValue1& v1, const TValue2& v2) {
        return Difference<TValue1, TValue2>{ v1, v2 };
    }

    template <Size Order>
    struct OuterProduct {
        static constexpr Size ORDER = Order;

        const Vector& v;

        template <Size I, Size J, Size... Is>
        INLINE constexpr Float value() const {
            static_assert(sizeof...(Is) + 2 == Order, "Invalid number of indices");
            return v[I] * OuterProduct<Order - 1>{ v }.template value<J, Is...>();
        }

        template <Size I>
        INLINE constexpr Float value() const {
            static_assert(Order == 1, "Invalid number of indices");
            return v[I];
        }
    };
}

template <Size N>
struct MultipoleExpansion {
    TracelessMultipole<N> Qn;
    MultipoleExpansion<N - 1> lower;

    template <Size M>
    INLINE std::enable_if_t<M != N, TracelessMultipole<M>&> order() {
        return lower.template order<M>();
    }
    template <Size M>
    INLINE std::enable_if_t<M != N, const TracelessMultipole<M>&> order() const {
        return lower.template order<M>();
    }
    template <Size M>
    INLINE std::enable_if_t<M == N, TracelessMultipole<M>&> order() {
        return Qn;
    }
    template <Size M>
    INLINE std::enable_if_t<M == N, const TracelessMultipole<M>&> order() const {
        return Qn;
    }
};

template <>
struct MultipoleExpansion<0> {
    TracelessMultipole<0> Qn;

    template <Size M>
    INLINE TracelessMultipole<0>& order() {
        static_assert(M == 0, "Invalid index");
        return Qn;
    }

    template <Size M>
    INLINE const TracelessMultipole<0>& order() const {
        static_assert(M == 0, "Invalid index");
        return Qn;
    }
};


/*template <Size M, Size N>
INLINE const TracelessMultipole<M>& getOrder(const MultipoleExpansion<N>& ms) {
    return getOrder<M>(ms.lower);
}
template <Size M, Size N>
INLINE TracelessMultipole<M>& getOrder(MultipoleExpansion<N>& ms) {
    return getOrder<M>(ms.lower);
}
template <Size M>
INLINE const TracelessMultipole<M>& getOrder(const MultipoleExpansion<M>& ms) {
    return ms.Qn;
}
template <Size M>
INLINE TracelessMultipole<M>& getOrder(MultipoleExpansion<M>& ms) {
    return ms.Qn;
}*/
/*
template <>
class Multipole<3> {
private:
    Vector data[3];


    Float components[COMPONENT_CNT];

public:
    Multipole() = default;

    Multipole(const Multipole& other) = default;

    Multipole(const Float value) {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            components[i] = value;
        }
    }

    template <typename... TArgs>
    INLINE Float& value()(const Size idx, const TArgs... rest) {
        static_assert(sizeof...(T1Args) == Order - 1, "Number of indices must be equal to multipole
order");
        ASSERT(min(rest...) >= idx);
        switch (idx) {
        case Z:
            // other components must be all Zs
            return components[COMPONENT_CNT - 1];
        }
    }

    /// Multiplication by scalar
    INLINE Multipole operator*(const Float f) const {
        return Multipole(data[0] * f, data[1] * f, data[2] * f);
    }

    /// Inner product with a vector
    INLINE Multipole<Order - 1> operator*(const Vector& v) {
        return v[0] * data[0] + v[1] * data[1] + v[2] * data[2];
    }

    template <typename... TArgs>
    INLINE Float apply(const Vector& v1, const TArgs&... rest) {
        static_assert(sizeof...(TArgs) == Order - 1, "Number of vectors must be equal to multipole
order");
        return (*this * v1).apply(rest...);
    }

    INLINE Multipole operator+(const Multipole& other) {
        return Multipole(data[0] + other.data[0], data[1] + other.data[1], data[2] + other.data[2]);
    }

    INLINE Multipole& operator+=(const Multipole& other) {
        data[0] += other.data[0];
        data[1] += other.data[1];
        data[2] += other.data[2];
        return *this;
    }

    bool operator==(const Multipole& other) const {
        return data[0] == other.data[0] && data[1] == other.data[1] && data[2] == other.data[2];
    }

    friend std::ostream& operator<<(std::ostream& stream, const Multipole& t) {
        stream << t.data[0] << std::endl;
        stream << t.data[1] << std::endl;
        stream << t.data[2] << std::endl << std::endl;
        return stream;
    }
};
*/

/*
template <>
class Multipole<0> {
private:
    Float data;

public:
    Multipole() = default;

    Multipole(const Multipole& other) = default;

    Multipole(const Float& v)
        : data(v) {}

    INLINE Float& value() {
        return data;
    }

    INLINE const Float& value() const {
        return data;
    }

    INLINE Multipole operator*(const Float f) const {
        return data * f;
    }

    INLINE Multipole operator+(const Multipole& other) {
        return data + other.data;
    }

    INLINE Multipole& operator+=(const Multipole& other) {
        data += other.data;
        return *this;
    }

    INLINE bool operator==(const Multipole& other) const {
        return data == other.data;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Multipole& t) {
        stream << t.data;
        return stream;
    }
};

/// Specialization for dipole = vector
template <>
class Multipole<1> {
private:
    Vector data;

public:
    Multipole() = default;

    Multipole(const Multipole& other) = default;

    Multipole(const Vector& v)
        : data(v) {}

    Multipole(const Float m1, const Float m2, const Float m3)
        : data{ m1, m2, m3 } {}

    Multipole(const Float value)
        : data{ value, value, value } {}

    INLINE Multipole<0>& operator[](const Size idx) {
        ASSERT(unsigned(idx) < 3);
        return (Multipole<0>&)data[idx];
    }

    INLINE Vector& value() {
        return data;
    }

    INLINE const Vector& value() const {
        return data;
    }

    INLINE Multipole<0> operator[](const Size idx) const {
        ASSERT(unsigned(idx) < 3);
        return data[idx];
    }

    INLINE Float& operator()(const Size idx) {
        return data[idx];
    }

    INLINE Multipole operator*(const Float f) const {
        return data * f;
    }

    INLINE Float operator*(const Vector& v) {
        return dot(data, v);
    }

    INLINE Float apply(const Vector& v) {
        return dot(data, v);
    }

    INLINE Multipole operator+(const Multipole& other) {
        return data + other.data;
    }

    INLINE Multipole& operator+=(const Multipole& other) {
        data += other.data;
        return *this;
    }

    INLINE bool operator==(const Multipole& other) const {
        return data == other.data;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Multipole& t) {
        stream << t.data;
        return stream;
    }
};*/

namespace Experimental {
    template <Size N>
    class MultipoleBase {
    protected:
        static constexpr Size COMPONENT_CNT = (N + 1) * (N + 2) / 2;

        Float data[COMPONENT_CNT];

    public:
        // inner
    };
}


NAMESPACE_SPH_END
