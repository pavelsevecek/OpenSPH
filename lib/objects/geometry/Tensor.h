#pragma once

/// \file Tensor.h
/// \brief Generic tensor of the 2nd order
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

class Tensor {
private:
    Vector v[3]; // rows

public:
    Tensor() = default;

    Tensor(const Float value)
        : v{ Vector(value), Vector(value), Vector(value) } {}

    /// Construct the matrix from vectors as rows
    Tensor(const Vector& v1, const Vector& v2, const Vector& v3)
        : v{ v1, v2, v3 } {}

    /// \param i Row index
    /// \param j Column index
    INLINE Float& operator()(const Size i, const Size j) {
        ASSERT(i < 3 && j < 3, i, j);
        return v[i][j];
    }

    INLINE Float operator()(const Size i, const Size j) const {
        ASSERT(i < 3 && j < 3, i, j);
        return v[i][j];
    }

    INLINE Vector column(const Size idx) const {
        ASSERT(idx < 3, idx);
        return Vector(v[0][idx], v[1][idx], v[2][idx]);
    }

    INLINE Vector row(const Size idx) const {
        ASSERT(idx < 3, idx);
        return v[idx];
    }

    INLINE Tensor transpose() const {
        return Tensor(column(0), column(1), column(2));
    }

    INLINE Float determinant() const {
        return v[0][0] * (v[1][1] * v[2][2] - v[2][1] * v[1][2]) -
               v[0][1] * (v[1][0] * v[2][2] - v[1][2] * v[2][0]) +
               v[0][2] * (v[1][0] * v[2][1] - v[1][1] * v[2][0]);
    }

    Tensor inverse() const {
        const Float det = this->determinant();
        ASSERT(det != 0._f);
        const Float invdet = 1._f / det;

        Tensor inv;
        inv(0, 0) = (v[1][1] * v[2][2] - v[2][1] * v[1][2]) * invdet;
        inv(0, 1) = (v[0][2] * v[2][1] - v[0][1] * v[2][2]) * invdet;
        inv(0, 2) = (v[0][1] * v[1][2] - v[0][2] * v[1][1]) * invdet;
        inv(1, 0) = (v[1][2] * v[2][0] - v[1][0] * v[2][2]) * invdet;
        inv(1, 1) = (v[0][0] * v[2][2] - v[0][2] * v[2][0]) * invdet;
        inv(1, 2) = (v[1][0] * v[0][2] - v[0][0] * v[1][2]) * invdet;
        inv(2, 0) = (v[1][0] * v[2][1] - v[2][0] * v[1][1]) * invdet;
        inv(2, 1) = (v[2][0] * v[0][1] - v[0][0] * v[2][1]) * invdet;
        inv(2, 2) = (v[0][0] * v[1][1] - v[1][0] * v[0][1]) * invdet;
        return inv;
    }

    static Tensor null() {
        return Tensor(0._f);
    }

    static Tensor identity() {
        return Tensor(Vector(1._f, 0._f, 0._f), Vector(0._f, 1._f, 0._f), Vector(0._f, 0._f, 1._f));
    }

    static Tensor rotateZ(const Float angle) {
        const Float c = cos(angle);
        const Float s = sin(angle);
        return Tensor(Vector(c, -s, 0._f), Vector(s, c, 0._f), Vector(0._f, 0._f, 1._f));
    }

    INLINE Tensor operator+(const Tensor& other) const {
        return Tensor(v[0] + other.v[0], v[1] + other.v[1], v[2] + other.v[2]);
    }

    INLINE Tensor operator-(const Tensor& other) const {
        return Tensor(v[0] - other.v[0], v[1] - other.v[1], v[2] - other.v[2]);
    }

    /// Matrix multiplication
    INLINE Tensor operator*(const Tensor& other) const {
        Tensor result;
        for (Size i = 0; i < 3; ++i) {
            for (Size j = 0; j < 3; ++j) {
                result(i, j) = dot(this->row(i), other.column(j));
            }
        }
        return result;
    }

    INLINE Vector operator*(const Vector& u) const {
        return Vector(dot(v[0], u), dot(v[1], u), dot(v[2], u));
    }

    INLINE friend Tensor operator*(const Tensor& t, const Float v) {
        return Tensor(t.row(0) * v, t.row(1) * v, t.row(2) * v);
    }

    INLINE friend Tensor operator*(const Float v, const Tensor& t) {
        return t * v;
    }

    INLINE Tensor& operator+=(const Tensor& other) {
        v[0] += other.v[0];
        v[1] += other.v[1];
        v[2] += other.v[2];
        return *this;
    }

    INLINE Tensor& operator-=(const Tensor& other) {
        v[0] -= other.v[0];
        v[1] -= other.v[1];
        v[2] -= other.v[2];
        return *this;
    }

    /// \todo test
    INLINE Tensor& operator*=(const Float value) {
        v[0] *= value;
        v[1] *= value;
        v[2] *= value;
        return *this;
    }

    INLINE Tensor& operator/=(const Float value) {
        ASSERT(value != 0._f);
        v[0] /= value;
        v[1] /= value;
        v[2] /= value;
        return *this;
    }

    INLINE bool operator==(const Tensor& other) const {
        return v[0] == other.v[0] && v[1] == other.v[1] && v[2] == other.v[2];
    }

    INLINE bool operator!=(const Tensor& other) const {
        return !(*this == other);
    }

    friend std::ostream& operator<<(std::ostream& stream, const Tensor& t) {
        stream << t.row(0) << t.row(1) << t.row(2);
        return stream;
    }
};


/// Checks if two tensors are equal to some given accuracy.
INLINE bool almostEqual(const Tensor& t1, const Tensor& t2, const Float eps = EPS) {
    return almostEqual(t1.row(0), t2.row(0), eps) && almostEqual(t1.row(1), t2.row(1), eps) &&
           almostEqual(t1.row(2), t2.row(2), eps);
}

/// Arbitrary norm of the tensor.
/// \todo Use some well-defined norm instead? (spectral norm, L1 or L2 norm, ...)
template <>
INLINE Float norm(const Tensor& t) {
    const Vector v = max(t.row(0), t.row(1), t.row(2));
    ASSERT(isReal(v));
    return norm(v);
}

/// Arbitrary squared norm of the tensor
template <>
INLINE Float normSqr(const Tensor& t) {
    const Vector v = max(t.row(0), t.row(1), t.row(2));
    return normSqr(v);
}

/// Returns the tensor of absolute values
template <>
INLINE auto abs(const Tensor& t) {
    return Tensor(abs(t.row(0)), abs(t.row(1)), abs(t.row(2)));
}

/// Returns the minimal element of the tensor.
template <>
INLINE Float minElement(const Tensor& t) {
    return min(minElement(t.row(0)), minElement(t.row(1)), minElement(t.row(2)));
}

/// Component-wise minimum of two tensors.
template <>
INLINE Tensor min(const Tensor& t1, const Tensor& t2) {
    return Tensor(min(t1.row(0), t2.row(0)), min(t1.row(1), t2.row(1)), min(t1.row(2), t2.row(2)));
}

/// Component-wise maximum of two tensors.
template <>
INLINE Tensor max(const Tensor& t1, const Tensor& t2) {
    return Tensor(max(t1.row(0), t2.row(0)), max(t1.row(1), t2.row(1)), max(t1.row(2), t2.row(2)));
}

/// Clamping all components by range.
template <>
INLINE Tensor clamp(const Tensor& t, const Range& range) {
    return Tensor(clamp(t.row(0), range), clamp(t.row(1), range), clamp(t.row(2), range));
}

template <>
INLINE bool isReal(const Tensor& t) {
    return isReal(t.row(0)) && isReal(t.row(1)) && isReal(t.row(2));
}

template <>
INLINE StaticArray<Float, 6> getComponents(const Tensor&) {
    NOT_IMPLEMENTED;
}


NAMESPACE_SPH_END
