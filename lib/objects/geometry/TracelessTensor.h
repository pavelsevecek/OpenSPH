#pragma once

/// \file TracelessTensor.h
/// \brief Symmetric traceless 2nd order tensor
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/geometry/SymmetricTensor.h"

NAMESPACE_SPH_BEGIN

class TracelessTensor {
    template <typename T>
    friend Float minElement(const T& t);
    template <typename T>
    friend auto abs(const T& t);
    template <typename T>
    friend T sqrtInv(const T& t);
    template <typename T>
    friend constexpr T min(const T& t1, const T& t2);
    template <typename T>
    friend constexpr T max(const T& t1, const T& t2);
    template <typename T>
    friend T clamp(const T& t, const Range& range);
    template <typename T>
    friend constexpr auto less(const T& t1, const T& t2);

private:
    // 5 independent components: 4 in vector, 1 in float
    enum Ids {
        M00,
        M11,
        M01,
        M02 // order of components in vector
    };
    Vector m;
    Float m12;

    INLINE TracelessTensor(const Vector& m, const Float m12)
        : m(m)
        , m12(m12) {}

public:
    INLINE TracelessTensor() = default;

    INLINE TracelessTensor(const TracelessTensor& other)
        : m(other.m)
        , m12(other.m12) {}

    /// Construct traceless tensor using other tensor (not traceless in general). "Tracelessness" of the
    /// tensor is checked by assert.
    INLINE explicit TracelessTensor(const SymmetricTensor& other) {
        m = other.diagonal();
        const Vector off = other.offDiagonal();
        m[M01] = off[0];
        m[M02] = off[1];
        m12 = off[2];
        // assert after we set variables to shut up gcc's "maybe uninitialized" warning
        ASSERT(abs(other.trace()) <= 1.e-3_f * getLength(other.diagonal()) + EPS, *this, other);
    }

    /// Initialize all components of the tensor to given value, excluding last element of the diagonal, which
    /// is computed to keep the trace zero. This constructor is mainly used to create null tensor; for
    /// non-zero values should be used sparingly.
    INLINE explicit TracelessTensor(const Float value)
        : m(value)
        , m12(value) {}

    /// Initialize tensor given 5 independent components.
    INLINE TracelessTensor(const Float xx, const Float yy, const Float xy, const Float xz, const Float yz)
        : m(xx, yy, xy, xz)
        , m12(yz) {}

    /// Construct tensor given three vectors as rows. Matrix represented by the vectors MUST be symmetric and
    /// traceless, checked by assert.
    INLINE TracelessTensor(const Vector& v0, const Vector& v1, const Vector& UNUSED_IN_RELEASE(v2)) {
        ASSERT(v0[1] == v1[0]);
        ASSERT(v0[2] == v2[0]);
        ASSERT(v1[2] == v2[1]);
        ASSERT(abs(v0[0] + v1[1] + v2[2]) < EPS * (norm(v0) + norm(v1) + norm(v2)));
        m = Vector(v0[0], v1[1], v0[1], v0[2]);
        m12 = v1[2];
    }

    INLINE TracelessTensor& operator=(const TracelessTensor& other) {
        m = other.m;
        m12 = other.m12;
        return *this;
    }

    INLINE TracelessTensor& operator=(const SymmetricTensor& other) {
        *this = TracelessTensor(other);
        return *this;
    }

    /// Conversion to ordinary SymmetricTensor
    INLINE explicit operator SymmetricTensor() const {
        return SymmetricTensor(Vector(m[M00], m[M11], -m[M00] - m[M11]), Vector(m[M01], m[M02], m12));
    }

    /// Returns a row of the matrix.
    INLINE Vector operator[](const int idx) const {
        ASSERT(unsigned(idx) < 3);
        switch (idx) {
        case 0:
            return Vector(m[M00], m[M01], m[M02]);
        case 1:
            return Vector(m[M01], m[M11], m12);
        case 2:
            return Vector(m[M02], m12, -m[M00] - m[M11]);
        default:
            STOP;
        }
    }

    /// Returns diagonal of the matrix
    INLINE Vector diagonal() const {
        return Vector(m[M00], m[M11], -m[M00] - m[M11]);
    }

    /// Returns off-diagonal elements of the matrix
    INLINE Vector offDiagonal() const {
        return Vector(m[M01], m[M02], m12);
    }

    /// Returns a given element of the matrix. Does NOT returns a reference last element of diagonal is always
    /// computed from others and is not stored in the object.
    INLINE Float operator()(const int rowIdx, const int colIdx) const {
        if (rowIdx == colIdx) {
            // diagonal
            if (rowIdx < 2) {
                return m[rowIdx];
            } else {
                return -m[0] - m[1];
            }
        } else {
            // off-diagonal
            const int sum = rowIdx + colIdx;
            if (sum < 3) {
                return m[sum + 1];
            } else {
                return m12;
            }
        }
    }

    /// Applies the tensor on given vector
    INLINE Vector operator*(const Vector& v) const {
        return Vector(m[M00] * v[0] + m[M01] * v[1] + m[M02] * v[2],
            m[M01] * v[0] + m[M11] * v[1] + m12 * v[2],
            m[M02] * v[0] + m12 * v[1] + (-m[M00] - m[M11]) * v[2]);
    }

    /// Multiplies the tensor by a scalar
    INLINE friend TracelessTensor operator*(const TracelessTensor& t, const Float v) {
        return TracelessTensor(t.m * v, t.m12 * v);
    }

    INLINE friend TracelessTensor operator*(const Float v, const TracelessTensor& t) {
        return TracelessTensor(t.m * v, t.m12 * v);
    }

    /// Multiplies a tensor by another tensor, element-wise. Not a matrix multiplication!
    /* INLINE friend TracelessTensor operator*(const TracelessTensor& t1, const TracelessTensor& t2) {
         return TracelessTensor(t1.m * t2.m, t1.m12 * t2.m12);
     }

     /// Divides a tensor by another tensor, element-wise.
     INLINE friend TracelessTensor operator/(const TracelessTensor& t1, const TracelessTensor& t2) {
         return TracelessTensor(t1.m / t2.m, t1.m12 / t2.m12);
     }*/

    /// Divides a tensor by a scalar
    INLINE friend TracelessTensor operator/(const TracelessTensor& t, const Float v) {
        return TracelessTensor(t.m / v, t.m12 / v);
    }

    /// \note product of two traceless tensor is NOT generally a traceless tensor


    INLINE friend TracelessTensor operator+(const TracelessTensor& t1, const TracelessTensor& t2) {
        return TracelessTensor(t1.m + t2.m, t1.m12 + t2.m12);
    }

    INLINE friend TracelessTensor operator-(const TracelessTensor& t1, const TracelessTensor& t2) {
        return TracelessTensor(t1.m - t2.m, t1.m12 - t2.m12);
    }

    INLINE TracelessTensor& operator+=(const TracelessTensor& other) {
        m += other.m;
        m12 += other.m12;
        return *this;
    }

    INLINE TracelessTensor& operator-=(const TracelessTensor& other) {
        m -= other.m;
        m12 -= other.m12;
        return *this;
    }

    INLINE TracelessTensor& operator*=(const Float value) {
        m *= value;
        m12 *= value;
        return *this;
    }

    INLINE TracelessTensor& operator/=(const Float value) {
        m /= value;
        m12 /= value;
        return *this;
    }

    INLINE TracelessTensor operator-() const {
        return TracelessTensor(-m, -m12);
    }

    INLINE bool operator==(const TracelessTensor& other) const {
        return m == other.m && m12 == other.m12;
    }

    INLINE friend bool operator==(const TracelessTensor& t1, const SymmetricTensor& t2) {
        return t1.diagonal() == t2.diagonal() && t1.offDiagonal() == t2.offDiagonal();
    }

    INLINE friend bool operator==(const SymmetricTensor& t1, const TracelessTensor& t2) {
        return t1.diagonal() == t2.diagonal() && t1.offDiagonal() == t2.offDiagonal();
    }

    INLINE bool operator!=(const TracelessTensor& other) const {
        return m != other.m || m12 != other.m12;
    }

    INLINE friend bool operator!=(const TracelessTensor& t1, const SymmetricTensor& t2) {
        return t1.diagonal() != t2.diagonal() || t1.offDiagonal() != t2.offDiagonal();
    }

    INLINE friend bool operator!=(const SymmetricTensor& t1, const TracelessTensor& t2) {
        return t1.diagonal() != t2.diagonal() || t1.offDiagonal() != t2.offDiagonal();
    }

    /// Returns a tensor with all zeros.
    INLINE static TracelessTensor null() {
        return TracelessTensor(0._f);
    }

    friend std::ostream& operator<<(std::ostream& stream, const TracelessTensor& t) {
        stream << std::setprecision(6) << std::setw(20) << t(0, 0) << std::setw(20) << t(1, 1)
               << std::setw(20) << t(0, 1) << std::setw(20) << t(0, 2) << std::setw(20) << t(1, 2);
        return stream;
    }
};

static_assert(sizeof(TracelessTensor) == 6 * sizeof(Float), "Incorrect size of TracelessTensor");

/// Traceless tensor utils

/// Checks if two tensors are equal to some given accuracy.
INLINE bool almostEqual(const TracelessTensor& t1, const TracelessTensor& t2, const Float eps = EPS) {
    return almostEqual(t1.diagonal(), t2.diagonal(), eps) &&
           almostEqual(t1.offDiagonal(), t2.offDiagonal(), eps);
}

/// Arbitrary norm of the tensor.
/// \todo Use some well-defined norm instead? (spectral norm, L1 or L2 norm, ...)
template <>
INLINE Float norm(const TracelessTensor& t) {
    /// \todo optimize
    const Vector v = max(t.diagonal(), t.offDiagonal());
    ASSERT(isReal(v));
    return norm(v);
}

/// Arbitrary squared norm of the tensor
template <>
INLINE Float normSqr(const TracelessTensor& t) {
    const Vector v = max(t.diagonal(), t.offDiagonal());
    ASSERT(isReal(v));
    return normSqr(v);
}

/// Returns the minimal component of the traceless tensor.
template <>
INLINE Float minElement(const TracelessTensor& t) {
    /// \todo optimize
    const Float vectorMin = min(min(t.m[0], t.m[1]), min(t.m[2], t.m[3]));
    const Float result = min(vectorMin, t.m12, -t.m[0] - t.m[1]);
    ASSERT(isReal(result) && result <= 0._f);
    return result;
}

/// Returns the tensor of absolute values form traceless tensor elements.. This yields a tensor with nonzero
/// trace (unless the tensor has zero diagonal elements).
template <>
INLINE auto abs(const TracelessTensor& t) {
    return SymmetricTensor(abs(t.diagonal()), abs(t.offDiagonal()));
}

template <>
INLINE TracelessTensor sqrtInv(const TracelessTensor&) {
    NOT_IMPLEMENTED
}

/// Component-wise minimum of two tensors.
template <>
INLINE TracelessTensor min(const TracelessTensor& t1, const TracelessTensor& t2) {
    return TracelessTensor(min(t1.m, t2.m), min(t1.m12, t2.m12));
}

/// Component-wise maximum of two tensors.
template <>
INLINE TracelessTensor max(const TracelessTensor& t1, const TracelessTensor& t2) {
    return TracelessTensor(max(t1.m, t2.m), max(t1.m12, t2.m12));
}


template <>
INLINE auto less(const TracelessTensor& t1, const TracelessTensor& t2) {
    return SymmetricTensor(less(t1.diagonal(), t2.diagonal()), less(t1.offDiagonal(), t2.offDiagonal()));
}

/// Clamps components of the traceless tensor. To preserve invariant (zero trace), the components are clamped
/// and the trace of the clamped tensor is subtracted from the result. Diagonal components of the traceless
/// tensor can therefore be changed even if they lie within the range.
/// For exact clamping of tensor components, the traceless tensor must be explicitly casted to Tensor.
template <>
INLINE TracelessTensor clamp(const TracelessTensor& t, const Range& range) {
    const SymmetricTensor clamped(clamp(t.diagonal(), range), clamp(t.offDiagonal(), range));
    return TracelessTensor(clamped - SymmetricTensor::identity() * clamped.trace() / 3._f);
}

template <>
INLINE Pair<TracelessTensor> clampWithDerivative(const TracelessTensor& v,
    const TracelessTensor& dv,
    const Range& range) {
    const SymmetricTensor lower = less(SymmetricTensor(range.lower()), SymmetricTensor(v));
    const SymmetricTensor upper = less(SymmetricTensor(v), SymmetricTensor(range.upper()));
    /// \todo optimize
    return { clamp(v, range), TracelessTensor(SymmetricTensor(dv) * lower * upper) };
}

template <>
INLINE bool isReal(const TracelessTensor& t) {
    return isReal(t.diagonal()) && isReal(t.offDiagonal());
}

/// Double-dot product t1 : t2 = sum_ij t1_ij t2_ij
INLINE Float ddot(const TracelessTensor& t1, const SymmetricTensor& t2) {
    return dot(t1.diagonal(), t2.diagonal()) + 2._f * dot(t1.offDiagonal(), t2.offDiagonal());
}

INLINE Float ddot(const SymmetricTensor& t1, const TracelessTensor& t2) {
    return dot(t1.diagonal(), t2.diagonal()) + 2._f * dot(t1.offDiagonal(), t2.offDiagonal());
}

INLINE Float ddot(const TracelessTensor& t1, const TracelessTensor& t2) {
    return dot(t1.diagonal(), t2.diagonal()) + 2._f * dot(t1.offDiagonal(), t2.offDiagonal());
}

template <>
INLINE StaticArray<Float, 6> getComponents(const TracelessTensor& t) {
    return { t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(0, 2), t(1, 2) };
}

NAMESPACE_SPH_END
