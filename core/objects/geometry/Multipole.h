#pragma once

#include "math/MathUtils.h"
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
struct MultipoleMappingImpl<0, Second, Idxs...> {
    static constexpr Size value = MultipoleMappingImpl<Second, Idxs...>::value;
};
template <Size I>
struct MultipoleMappingImpl<I> {
    static constexpr Size value = I;
};
template <Size Second, Size... Idxs>
struct MultipoleMappingImpl<1, Second, Idxs...> {
    static constexpr Size value =
        multipoleComponentCnt(sizeof...(Idxs) + 1) - 1 - sizeof...(Idxs) + sum<Second, Idxs...>();
};
template <Size Second, Size... Idxs>
struct MultipoleMappingImpl<2, Second, Idxs...> {
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
} // namespace Detail

template <Size Order>
class Multipole {
private:
    static constexpr Size COMPONENT_CNT = Detail::multipoleComponentCnt(Order);

    Float data[COMPONENT_CNT];

public:
    static constexpr Size ORDER = Order;

    constexpr Multipole()
        : data{ 0 } {}

    constexpr Multipole(const Float f) {
        for (Float& v : data) {
            v = f;
        }
    }

    template <Size... Idxs>
    INLINE constexpr Float value() const {
        static_assert(sizeof...(Idxs) == Order, "Number of indices must match the order");
        const Size idx = Detail::MultipoleMapping<Idxs...>::value;
        SPH_ASSERT(idx < COMPONENT_CNT, idx);
        return *(&data[0] + idx);
    }

    template <Size... Idxs>
    INLINE constexpr Float& value() {
        static_assert(sizeof...(Idxs) == Order, "Number of indices must match the order");
        const Size idx = Detail::MultipoleMapping<Idxs...>::value;
        SPH_ASSERT(idx < COMPONENT_CNT, idx);
        return *(&data[0] + idx);
    }

    INLINE constexpr Float operator[](const Size idx) const {
        SPH_ASSERT(idx < COMPONENT_CNT, idx);
        return data[idx];
    }

    constexpr Multipole& operator+=(const Multipole& other) {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            data[i] += other[i];
        }
        return *this;
    }

    constexpr bool operator==(const Multipole& other) const {
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
    static constexpr Multipole<0> make(const TValue& v) {
        Multipole<0> m;
        m.value() = v.value();
        return m;
    }
};

template <>
struct MakeMultipole<1> {
    template <typename TValue>
    static constexpr Multipole<1> make(const TValue& v) {
        Multipole<1> m;
        m.value<0>() = v.template value<0>();
        m.value<1>() = v.template value<1>();
        m.value<2>() = v.template value<2>();
        return m;
    }
};

template <>
struct MakeMultipole<2> {
    template <typename TValue>
    static constexpr Multipole<2> make(const TValue& v) {
        Multipole<2> m;
        m.value<0, 0>() = v.template value<0, 0>();
        m.value<0, 1>() = v.template value<0, 1>();
        m.value<0, 2>() = v.template value<0, 2>();

        m.value<1, 1>() = v.template value<1, 1>();
        m.value<1, 2>() = v.template value<1, 2>();

        m.value<2, 2>() = v.template value<2, 2>();
        return m;
    }
};

template <>
struct MakeMultipole<3> {
    template <typename TValue>
    static constexpr Multipole<3> make(const TValue& v) {
        Multipole<3> m;
        m.value<0, 0, 0>() = v.template value<0, 0, 0>();
        m.value<0, 0, 1>() = v.template value<0, 0, 1>();
        m.value<0, 0, 2>() = v.template value<0, 0, 2>();
        m.value<0, 1, 1>() = v.template value<0, 1, 1>();
        m.value<0, 1, 2>() = v.template value<0, 1, 2>();
        m.value<0, 2, 2>() = v.template value<0, 2, 2>();

        m.value<1, 1, 1>() = v.template value<1, 1, 1>();
        m.value<1, 1, 2>() = v.template value<1, 1, 2>();
        m.value<1, 2, 2>() = v.template value<1, 2, 2>();

        m.value<2, 2, 2>() = v.template value<2, 2, 2>();
        return m;
    }
};

template <>
struct MakeMultipole<4> {
    template <typename TValue>
    static constexpr Multipole<4> make(const TValue& v) {
        Multipole<4> m;
        m.value<0, 0, 0, 0>() = v.template value<0, 0, 0, 0>();
        m.value<0, 0, 0, 1>() = v.template value<0, 0, 0, 1>();
        m.value<0, 0, 0, 2>() = v.template value<0, 0, 0, 2>();
        m.value<0, 0, 1, 1>() = v.template value<0, 0, 1, 1>();
        m.value<0, 0, 1, 2>() = v.template value<0, 0, 1, 2>();
        m.value<0, 0, 2, 2>() = v.template value<0, 0, 2, 2>();
        m.value<0, 1, 1, 1>() = v.template value<0, 1, 1, 1>();
        m.value<0, 1, 1, 2>() = v.template value<0, 1, 1, 2>();
        m.value<0, 1, 2, 2>() = v.template value<0, 1, 2, 2>();
        m.value<0, 2, 2, 2>() = v.template value<0, 2, 2, 2>();

        m.value<1, 1, 1, 1>() = v.template value<1, 1, 1, 1>();
        m.value<1, 1, 1, 2>() = v.template value<1, 1, 1, 2>();
        m.value<1, 1, 2, 2>() = v.template value<1, 1, 2, 2>();
        m.value<1, 2, 2, 2>() = v.template value<1, 2, 2, 2>();

        m.value<2, 2, 2, 2>() = v.template value<2, 2, 2, 2>();
        return m;
    }
};

template <Size N, typename TValue>
constexpr Multipole<N> makeMultipole(const TValue& v) {
    return MakeMultipole<N>::make(v);
}


namespace Detail {
constexpr Size tracelessMultipoleComponentCnt(Size order) {
    return 2 * order + 1;
}

template <Size... Idxs>
struct TracelessMultipoleMappingImpl;

template <Size Second, Size... Idxs>
struct TracelessMultipoleMappingImpl<0, Second, Idxs...> {
    static constexpr Size value = TracelessMultipoleMappingImpl<Second, Idxs...>::value;
};
template <Size Second, Size... Idxs>
struct TracelessMultipoleMappingImpl<1, Second, Idxs...> {
    static constexpr Size value =
        tracelessMultipoleComponentCnt(sizeof...(Idxs) + 1) - 1 - sizeof...(Idxs) + sum<Second, Idxs...>();
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
} // namespace Detail

template <Size... Idxs>
struct TracelessMultipoleComponent;

template <Size Order>
class TracelessMultipole {
private:
    static constexpr Size COMPONENT_CNT = Detail::tracelessMultipoleComponentCnt(Order);

    Float data[COMPONENT_CNT];

public:
    static constexpr Size ORDER = Order;

    constexpr TracelessMultipole()
        : data{ 0 } {}

    constexpr TracelessMultipole(const Float f) {
        for (Float& v : data) {
            v = f;
        }
    }

    template <Size... Idxs>
    INLINE constexpr Float value() const {
        return TracelessMultipoleComponent<Idxs...>{}.get(*this);
    }

    template <Size... Idxs>
    INLINE constexpr Float& value() {
        static_assert(sizeof...(Idxs) == Order, "Number of indices must match the order");
        const Size idx = Detail::TracelessMultipoleMapping<Idxs...>::value;
        SPH_ASSERT(idx < COMPONENT_CNT, idx);
        return *(&data[0] + idx);
    }

    template <Size... Idxs>
    INLINE constexpr Float valueImpl() const {
        static_assert(sizeof...(Idxs) == Order, "Number of indices must match the order");
        const Size idx = Detail::TracelessMultipoleMapping<Idxs...>::value;
        SPH_ASSERT(idx < COMPONENT_CNT, idx, Idxs...);
        return *(&data[0] + idx);
    }

    INLINE constexpr Float operator[](const Size idx) const {
        SPH_ASSERT(idx < COMPONENT_CNT, idx);
        return data[idx];
    }

    INLINE constexpr Float& operator[](const Size idx) {
        SPH_ASSERT(idx < COMPONENT_CNT, idx);
        return data[idx];
    }

    INLINE constexpr Size size() const {
        return COMPONENT_CNT;
    }

    constexpr TracelessMultipole& operator+=(const TracelessMultipole& other) {
        for (Size i = 0; i < COMPONENT_CNT; ++i) {
            data[i] += other.data[i];
        }
        return *this;
    }

    constexpr bool operator==(const TracelessMultipole& other) const {
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
class TracelessMultipole<0> {
private:
    Float data;

public:
    constexpr TracelessMultipole()
        : data(0) {}

    constexpr INLINE TracelessMultipole(const Float v)
        : data(v) {}

    static constexpr Size ORDER = 0;

    INLINE constexpr Float& value() {
        return data;
    }

    INLINE constexpr Float value() const {
        return data;
    }

    INLINE constexpr Float operator[](const Size idx) const {
        SPH_ASSERT(idx == 0);
        return data;
    }

    INLINE constexpr Size size() const {
        return 1;
    }

    INLINE constexpr operator Float() const {
        return data;
    }

    constexpr TracelessMultipole& operator+=(const TracelessMultipole& other) {
        data += other.data;
        return *this;
    }

    constexpr bool operator==(const TracelessMultipole& other) const {
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
    static constexpr TracelessMultipole<0> make(const TValue& v) {
        TracelessMultipole<0> m;
        m.value() = v.value();
        return m;
    }
};

template <>
struct MakeTracelessMultipole<1> {
    template <typename TValue>
    static constexpr TracelessMultipole<1> make(const TValue& v) {
        TracelessMultipole<1> m;
        m.value<0>() = v.template value<0>();
        m.value<1>() = v.template value<1>();
        m.value<2>() = v.template value<2>();
        return m;
    }
};

template <>
struct MakeTracelessMultipole<2> {
    template <typename TValue>
    static constexpr TracelessMultipole<2> make(const TValue& v) {
        TracelessMultipole<2> m;
        m.value<0, 0>() = v.template value<0, 0>();
        m.value<0, 1>() = v.template value<0, 1>();
        m.value<0, 2>() = v.template value<0, 2>();

        m.value<1, 1>() = v.template value<1, 1>();
        m.value<1, 2>() = v.template value<1, 2>();
        return m;
    }
};

template <>
struct MakeTracelessMultipole<3> {
    template <typename TValue>
    static constexpr TracelessMultipole<3> make(const TValue& v) {
        TracelessMultipole<3> m;
        m.value<0, 0, 0>() = v.template value<0, 0, 0>();
        m.value<0, 0, 1>() = v.template value<0, 0, 1>();
        m.value<0, 0, 2>() = v.template value<0, 0, 2>();
        m.value<0, 1, 1>() = v.template value<0, 1, 1>();
        m.value<0, 1, 2>() = v.template value<0, 1, 2>();

        m.value<1, 1, 1>() = v.template value<1, 1, 1>();
        m.value<1, 1, 2>() = v.template value<1, 1, 2>();
        return m;
    }
};

template <>
struct MakeTracelessMultipole<4> {
    template <typename TValue>
    static constexpr TracelessMultipole<4> make(const TValue& v) {
        TracelessMultipole<4> m;
        m.value<0, 0, 0, 0>() = v.template value<0, 0, 0, 0>();
        m.value<0, 0, 0, 1>() = v.template value<0, 0, 0, 1>();
        m.value<0, 0, 0, 2>() = v.template value<0, 0, 0, 2>();
        m.value<0, 0, 1, 1>() = v.template value<0, 0, 1, 1>();
        m.value<0, 0, 1, 2>() = v.template value<0, 0, 1, 2>();
        m.value<0, 1, 1, 1>() = v.template value<0, 1, 1, 1>();
        m.value<0, 1, 1, 2>() = v.template value<0, 1, 1, 2>();

        m.value<1, 1, 1, 1>() = v.template value<1, 1, 1, 1>();
        m.value<1, 1, 1, 2>() = v.template value<1, 1, 1, 2>();
        return m;
    }
};

template <Size N, typename TValue>
constexpr TracelessMultipole<N> makeTracelessMultipole(const TValue& v) {
    return MakeTracelessMultipole<N>::make(v);
}
template <Size... Idxs>
struct TracelessMultipoleComponentImpl {
    INLINE static constexpr Float get(const TracelessMultipole<sizeof...(Idxs)>& m) {
        return m.template valueImpl<Idxs...>();
    }
};
template <std::size_t... Idxs, std::size_t... Is>
constexpr Float expandTracelessMultipoleComponent(const TracelessMultipole<sizeof...(Idxs)>& m,
    std::index_sequence<Is...>) {
    constexpr ConstexprArray<Size, sizeof...(Idxs)> ar = sortIndices<Idxs...>();
    return TracelessMultipoleComponentImpl<ar[Is]...>::get(m);
}

// computes component from traceless multipole
template <Size... Idxs>
struct TracelessMultipoleComponent {
    using Sequence = std::make_index_sequence<sizeof...(Idxs)>;

    INLINE static constexpr Float get(const TracelessMultipole<sizeof...(Idxs)>& m) {
        return expandTracelessMultipoleComponent<Idxs...>(m, Sequence{});
    }
};

template <>
struct TracelessMultipoleComponentImpl<2, 2> {
    INLINE static constexpr Float get(const TracelessMultipole<2>& m) {
        return -m.valueImpl<0, 0>() - m.valueImpl<1, 1>();
    }
};
template <Size I>
struct TracelessMultipoleComponentImpl<I, 2, 2> {
    INLINE static constexpr Float get(const TracelessMultipole<3>& m) {
        return -m.valueImpl<0, 0, I>() - m.valueImpl<1, 1, I>();
    }
};
template <Size I, Size J>
struct TracelessMultipoleComponentImpl<I, J, 2, 2> {
    INLINE static constexpr Float get(const TracelessMultipole<4>& m) {
        return -m.valueImpl<0, 0, I, J>() - m.valueImpl<1, 1, I, J>();
    }
};


inline constexpr Size factorial(const Size n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
static_assert(factorial(1) == 1, "static test failed");
static_assert(factorial(2) == 2, "static test failed");
static_assert(factorial(3) == 6, "static test failed");
static_assert(factorial(4) == 24, "static test failed");

inline constexpr Size doubleFactorial(const Size n) {
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
    INLINE static constexpr int value() {
        static_assert(sizeof...(Rest) + 4 == Order, "Invalid number of indices");
        return ((I == J) ? 1 : 0) * Delta<Order - 2>::template value<K, L, Rest...>();
    }
    template <Size I, Size J>
    INLINE static constexpr int value() {
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
constexpr auto makePermutations(const Value1& v1, const Value2& v2) {
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
constexpr auto makeContraction(const TValue& v) {
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

template <typename TValue1, typename TValue2>
struct InnerProduct<3, 3, TValue1, TValue2> {
    const TValue1& v1;
    const TValue2& v2;

    template <Size... Is>
    INLINE constexpr Float value() const {
        static_assert(TValue1::ORDER == 3, "Invalid number of indices");
        static_assert(TValue2::ORDER == sizeof...(Is) + 3, "Invalid number of indices");
        return v1.template value<0, 0, 0>() * v2.template value<0, 0, 0, Is...>() +
               v1.template value<0, 0, 1>() * v2.template value<0, 0, 1, Is...>() +
               v1.template value<0, 0, 2>() * v2.template value<0, 0, 2, Is...>() +
               v1.template value<0, 1, 0>() * v2.template value<0, 1, 0, Is...>() +
               v1.template value<0, 1, 1>() * v2.template value<0, 1, 1, Is...>() +
               v1.template value<0, 1, 2>() * v2.template value<0, 1, 2, Is...>() +
               v1.template value<0, 2, 0>() * v2.template value<0, 2, 0, Is...>() +
               v1.template value<0, 2, 1>() * v2.template value<0, 2, 1, Is...>() +
               v1.template value<0, 2, 2>() * v2.template value<0, 2, 2, Is...>() +
               v1.template value<1, 0, 0>() * v2.template value<1, 0, 0, Is...>() +
               v1.template value<1, 0, 1>() * v2.template value<1, 0, 1, Is...>() +
               v1.template value<1, 0, 2>() * v2.template value<1, 0, 2, Is...>() +
               v1.template value<1, 1, 0>() * v2.template value<1, 1, 0, Is...>() +
               v1.template value<1, 1, 1>() * v2.template value<1, 1, 1, Is...>() +
               v1.template value<1, 1, 2>() * v2.template value<1, 1, 2, Is...>() +
               v1.template value<1, 2, 0>() * v2.template value<1, 2, 0, Is...>() +
               v1.template value<1, 2, 1>() * v2.template value<1, 2, 1, Is...>() +
               v1.template value<1, 2, 2>() * v2.template value<1, 2, 2, Is...>() +
               v1.template value<2, 0, 0>() * v2.template value<2, 0, 0, Is...>() +
               v1.template value<2, 0, 1>() * v2.template value<2, 0, 1, Is...>() +
               v1.template value<2, 0, 2>() * v2.template value<2, 0, 2, Is...>() +
               v1.template value<2, 1, 0>() * v2.template value<2, 1, 0, Is...>() +
               v1.template value<2, 1, 1>() * v2.template value<2, 1, 1, Is...>() +
               v1.template value<2, 1, 2>() * v2.template value<2, 1, 2, Is...>() +
               v1.template value<2, 2, 0>() * v2.template value<2, 2, 0, Is...>() +
               v1.template value<2, 2, 1>() * v2.template value<2, 2, 1, Is...>() +
               v1.template value<2, 2, 2>() * v2.template value<2, 2, 2, Is...>();
    }
};

template <Size N, typename TValue1, typename TValue2>
constexpr InnerProduct<N, TValue1::ORDER, TValue1, TValue2> makeInner(const TValue1& v1, const TValue2& v2) {
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
        static_assert(TValue1::ORDER == 1 && TValue2::ORDER == sizeof...(Is), "Invalid number of indices");
        return v1.template value<I>() * v2.template value<Is...>();
    }
};

template <typename TValue1, typename TValue2>
struct MultiplyTwo<2, TValue1, TValue2> {
    const TValue1& v1;
    const TValue2& v2;

    template <Size I, Size J, Size... Is>
    INLINE constexpr Float value() const {
        static_assert(TValue1::ORDER == 2 && TValue2::ORDER == sizeof...(Is), "Invalid number of indices");
        return v1.template value<I, J>() * v2.template value<Is...>();
    }
};

template <typename TValue1, typename TValue2>
struct MultiplyTwo<4, TValue1, TValue2> {
    const TValue1& v1;
    const TValue2& v2;

    template <Size I, Size J, Size K, Size L, Size... Is>
    INLINE constexpr Float value() const {
        static_assert(TValue1::ORDER == 4 && TValue2::ORDER == sizeof...(Is), "Invalid number of indices");
        return v1.template value<I, J, K, L>() * v2.template value<Is...>();
    }
};

template <typename TValue>
constexpr MultiplyByScalar<TValue> operator*(const TValue& v, const Float f) {
    return MultiplyByScalar<TValue>{ v, f };
}

template <typename TValue1, typename TValue2>
constexpr MultiplyTwo<TValue1::ORDER, TValue1, TValue2> makeMultiply(const TValue1& v1, const TValue2& v2) {
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
constexpr auto operator+(const TValue1& v1, const TValue2& v2) {
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
constexpr auto operator-(const TValue1& v1, const TValue2& v2) {
    return Difference<TValue1, TValue2>{ v1, v2 };
}

template <Size Order>
struct OuterProduct {
    static constexpr Size ORDER = Order;

    const Multipole<1>& v;

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
} // namespace MomentOperators

template <Size N>
struct MultipoleExpansion {
    TracelessMultipole<N> Qn;
    MultipoleExpansion<N - 1> lower;

    template <Size M>
    INLINE constexpr std::enable_if_t<M != N, TracelessMultipole<M>&> order() {
        return lower.template order<M>();
    }
    template <Size M>
    INLINE constexpr std::enable_if_t<M != N, const TracelessMultipole<M>&> order() const {
        return lower.template order<M>();
    }
    template <Size M>
    INLINE constexpr std::enable_if_t<M == N, TracelessMultipole<M>&> order() {
        return Qn;
    }
    template <Size M>
    INLINE constexpr std::enable_if_t<M == N, const TracelessMultipole<M>&> order() const {
        return Qn;
    }
    constexpr MultipoleExpansion multiply(const Float factor) const {
        MultipoleExpansion m = *this;
        m.lower = lower.multiply(factor);
        for (Size i = 0; i < Qn.size(); ++i) {
            m.Qn[i] *= factor;
        }
        return m;
    }
};

template <>
struct MultipoleExpansion<0> {
    TracelessMultipole<0> Qn;

    template <Size M>
    INLINE constexpr TracelessMultipole<0>& order() {
        static_assert(M == 0, "Invalid index");
        return Qn;
    }

    template <Size M>
    INLINE constexpr const TracelessMultipole<0>& order() const {
        static_assert(M == 0, "Invalid index");
        return Qn;
    }
    constexpr MultipoleExpansion multiply(const Float factor) const {
        MultipoleExpansion m = *this;
        m.Qn.value() *= factor;
        return m;
    }
};

NAMESPACE_SPH_END
