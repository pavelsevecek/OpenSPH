#pragma once

#include "geometry/Vector.h"

NAMESPACE_SPH_BEGIN

template <std::size_t Order>
class Multipole  {
private:
    using Previous = Multipole<Order - 1>;
    Previous data[3];

public:
    Multipole() = default;

    Multipole(const Multipole& other) = default;

    Multipole(Multipole&& other) = default;

    Multipole(const Multipole<Order - 1>& m1, const Multipole<Order - 1>& m2, const Multipole<Order - 1>& m3)
        : data{ m1, m2, m3 } {}

    Multipole(const Float value)
        : data{ value, value, value } {}

    INLINE Multipole<Order - 1>& operator[](const int idx) {
        ASSERT(unsigned(idx) < 3);
        return data[idx];
    }

    INLINE const Multipole<Order - 1>& operator[](const int idx) const {
        ASSERT(unsigned(idx) < 3);
        return data[idx];
    }

    /// Inner product with a vector
    INLINE Multipole<Order - 1> operator*(const Vector& v) {
        return v[0] * data[0] + v[1] * data[1] + v[2] * data[2];
    }

    template <typename... TArgs>
    INLINE Float apply(const Vector& v1, const TArgs&... rest) {
        static_assert(sizeof...(TArgs) == Order - 1, "Number of vectors must equal multipole order");
        return (*this * v1).apply(rest...);
    }

    INLINE Multipole operator+(const Multipole& other) {
        return Multipole(data[0] + other.data[0], data[1] + other.data[1], data[2] + other.data[2]);
    }
};

/// Specialization for dipole = vector
template <>
class Multipole<1> : public Vector {
public:
    using Vector::Vector;

    Multipole(const Vector& v)
        : Vector(v) {}

    INLINE Float apply(const Vector& v) { return dot(*this, v); }
};

NAMESPACE_SPH_END
