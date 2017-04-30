#pragma once

#include "geometry/Vector.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN

template <Size Order>
class Multipole {
private:
    Multipole<Order - 1> data[3];

public:
    Multipole() = default;

    Multipole(const Multipole& other) = default;

    Multipole(Multipole&& other) = default;

    Multipole(const Multipole<Order - 1>& m1, const Multipole<Order - 1>& m2, const Multipole<Order - 1>& m3)
        : data{ m1, m2, m3 } {}

    Multipole(const Float value)
        : data{ value, value, value } {}

    INLINE Multipole<Order - 1>& operator[](const Size idx) {
        ASSERT(unsigned(idx) < 3);
        return data[idx];
    }

    INLINE const Multipole<Order - 1>& operator[](const Size idx) const {
        ASSERT(unsigned(idx) < 3);
        return data[idx];
    }

    template <typename... TArgs>
    INLINE Float& operator()(const Size idx, const TArgs... rest) {
        static_assert(sizeof...(TArgs) == Order - 1, "Number of indices must be equal to multipole order");
        return data[idx](rest...);
    }

    /// Inner product with a vector
    INLINE Multipole<Order - 1> operator*(const Vector& v) {
        return v[0] * data[0] + v[1] * data[1] + v[2] * data[2];
    }

    template <typename... TArgs>
    INLINE Float apply(const Vector& v1, const TArgs&... rest) {
        static_assert(sizeof...(TArgs) == Order - 1, "Number of vectors must be equal to multipole order");
        return (*this * v1).apply(rest...);
    }

    INLINE Multipole operator+(const Multipole& other) {
        return Multipole(data[0] + other.data[0], data[1] + other.data[1], data[2] + other.data[2]);
    }

    bool operator==(const Multipole& other) const {
        return data[0] == other.data[0] && data[1] == other.data[1] && data[2] == other.data[2];
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

    Multipole(Multipole&& other) = default;

    Multipole(const Vector& v)
        : data(v) {}

    Multipole(const Float m1, const Float m2, const Float m3)
        : data{ m1, m2, m3 } {}

    Multipole(const Float value)
        : data{ value, value, value } {}

    INLINE Float& operator[](const Size idx) {
        ASSERT(unsigned(idx) < 3);
        return data[idx];
    }

    INLINE Float operator[](const Size idx) const {
        ASSERT(unsigned(idx) < 3);
        return data[idx];
    }

    INLINE Float& operator()(const Size idx) {
        return data[idx];
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

    INLINE bool operator==(const Multipole& other) const {
        return data == other.data;
    }
};

template <Size Order>
class Permutation {
private:
    Size data[Order]; // not using StaticArray for constexpr-ness

public:
    template <typename... TArgs>
    constexpr Permutation(const TArgs... args)
        : data{ args... } {}

    constexpr Size operator[](const Size idx) const {
        return data[idx];
    }
};

template <Size Order>
struct UniquePermutations;


NAMESPACE_SPH_END
