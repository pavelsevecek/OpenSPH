#pragma once

/// Basic algebra for symmetric 2nd order tensors
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz


#include "geometry/Vector.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN

class Tensor {
private:
    Vector diag; // diagonal part
    Vector off;  // over/below diagonal

public:
    Tensor() = default;

    INLINE Tensor(const Tensor& other)
        : diag(other.diag)
        , off(other.off) {}

    /// Construct tensor given its diagonal vector and a vector of off-diagonal elements (sorted top-bottom
    /// and left-right).
    INLINE Tensor(const Vector& diag, const Vector& off)
        : diag(diag)
        , off(off) {}

    /// Initialize all components of the tensor to given value.
    INLINE explicit Tensor(const Float value)
        : diag(value)
        , off(value) {}

    /// Construct tensor given three vectors as rows. Matrix represented by the vectors MUST be symmetric,
    /// checked by assert.
    INLINE Tensor(const Vector& v0, const Vector& v1, const Vector& v2) {
        ASSERT(v0[1] == v1[0]);
        ASSERT(v0[2] == v2[0]);
        ASSERT(v1[2] == v2[1]);
        diag = Vector(v0[0], v1[1], v2[2]);
        off = Vector(v0[1], v0[2], v1[2]);
    }

    INLINE Tensor& operator=(const Tensor& other) {
        diag = other.diag;
        off = other.off;
        return *this;
    }

    /// Returns a row of the matrix.
    INLINE Vector operator[](const Size idx) const {
        ASSERT(unsigned(idx) < 3);
        switch (idx) {
        case 0:
            return Vector(diag[0], off[0], off[1]);
        case 1:
            return Vector(off[0], diag[1], off[2]);
        case 2:
            return Vector(off[1], off[2], diag[2]);
        default:
            STOP;
        }
    }

    /// Returns a given element of the matrix.
    INLINE Float& operator()(const Size rowIdx, const Size colIdx) {
        if (rowIdx == colIdx) {
            return diag[rowIdx];
        } else {
            return off[rowIdx + colIdx - 1];
        }
    }

    /// Returns a given element of the matrix, const version.
    INLINE Float operator()(const Size rowIdx, const Size colIdx) const {
        if (rowIdx == colIdx) {
            return diag[rowIdx];
        } else {
            return off[rowIdx + colIdx - 1];
        }
    }

    /// Returns the diagonal part of the tensor
    INLINE const Vector& diagonal() const {
        return diag;
    }

    /// Returns the off-diagonal elements of the tensor
    INLINE const Vector& offDiagonal() const {
        return off;
    }

    /// Applies the tensor on given vector
    INLINE Vector operator*(const Vector& v) const {
        /// \todo optimize using _mm_shuffle_ps
        return Vector(diag[0] * v[0] + off[0] * v[1] + off[1] * v[2],
            off[0] * v[0] + diag[1] * v[1] + off[2] * v[2],
            off[1] * v[0] + off[2] * v[1] + diag[2] * v[2]);
    }

    /// Multiplies a tensor by a scalar
    INLINE friend Tensor operator*(const Tensor& t, const Float v) {
        return Tensor(t.diag * v, t.off * v);
    }

    INLINE friend Tensor operator*(const Float v, const Tensor& t) {
        return Tensor(t.diag * v, t.off * v);
    }

    /// Multiplies a tensor by another tensor, element-wise. Not a matrix multiplication!
    INLINE friend Tensor operator*(const Tensor& t1, const Tensor& t2) {
        return Tensor(t1.diag * t2.diag, t1.off * t2.off);
    }

    /// Divides a tensor by a scalar
    INLINE friend Tensor operator/(const Tensor& t, const Float v) {
        return Tensor(t.diag / v, t.off / v);
    }

    /// Divides a tensor by another tensor, element-wise.
    INLINE friend Tensor operator/(const Tensor& t1, const Tensor& t2) {
        return Tensor(t1.diag / t2.diag, t1.off / t2.off);
    }

    /// Sums up two tensors
    INLINE friend Tensor operator+(const Tensor& t1, const Tensor& t2) {
        return Tensor(t1.diag + t2.diag, t1.off + t2.off);
    }

    INLINE friend Tensor operator-(const Tensor& t1, const Tensor& t2) {
        return Tensor(t1.diag - t2.diag, t1.off - t2.off);
    }

    INLINE Tensor& operator+=(const Tensor& other) {
        diag += other.diag;
        off += other.off;
        return *this;
    }

    INLINE Tensor& operator-=(const Tensor& other) {
        diag -= other.diag;
        off -= other.off;
        return *this;
    }

    INLINE Tensor operator-() const {
        return Tensor(-diag, -off);
    }

    INLINE bool operator==(const Tensor& other) const {
        return diag == other.diag && off == other.off;
    }

    INLINE bool operator!=(const Tensor& other) const {
        return diag != other.diag || off != other.off;
    }

    /// Returns an identity tensor.
    INLINE static Tensor identity() {
        return Tensor(Vector(1._f, 1._f, 1._f), Vector(0._f, 0._f, 0._f));
    }

    /// Returns a tensor with all zeros.
    INLINE static Tensor null() {
        return Tensor(Vector(0._f), Vector(0._f));
    }

    /// Returns the determinant of the tensor
    INLINE Float determinant() const {
        return diag[0] * diag[1] * diag[2] + 2 * off[0] * off[1] * off[2] -
               (dot(sqr(off), Vector(diag[2], diag[1], diag[0])));
    }

    /// Return the trace of the tensor
    INLINE Float trace() const {
        return dot(diag, Vector(1._f));
    }

    /// Returns n-th invariant of the tensor (1<=n<=3)
    template <int n>
    INLINE Float invariant() const {
        switch (n) {
        case 1:
            return trace();
        case 2:
            /// \todo optimize with shuffle
            return getSqrLength(off) - (diag[1] * diag[2] + diag[2] * diag[0] + diag[0] * diag[1]);
        case 3:
            return determinant();
        default:
            STOP;
        }
    }

    INLINE Tensor inverse() const {
        const Float det = determinant();
        ASSERT(det != 0._f);
        Vector invDiag, invOff;
        /// \todo optimize using SSE
        invDiag[0] = diag[1] * diag[2] - sqr(off[2]);
        invDiag[1] = diag[2] * diag[0] - sqr(off[1]);
        invDiag[2] = diag[0] * diag[1] - sqr(off[0]);
        invOff[0] = off[1] * off[2] - diag[2] * off[0];
        invOff[1] = off[2] * off[0] - diag[1] * off[1];
        invOff[2] = off[0] * off[1] - diag[0] * off[2];
        return Tensor(invDiag / det, invOff / det);
    }

    static Tensor RotateX(const Float a);
    static Tensor RotateY(const Float a);
    static Tensor RotateZ(const Float a);
    static Tensor RotateEuler(const Float a, const Float b, const Float c);
    static Tensor RotateAxis(const Vector& axis, const Float a);
    static Tensor Translate(const Vector& t);
    static Tensor Scale(const Float x, const Float y, const Float z);
    static Tensor Scale(const Float s);
    static Tensor TRS(const Vector& t, const Vector& r, const Vector& s);

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Tensor& t) {
        stream << t.diagonal() << t.offDiagonal();
        return stream;
    }
};


/// Tensor utils


/// Checks if two tensors are equal to some given accuracy.
INLINE bool almostEqual(const Tensor& t1, const Tensor& t2, const Float eps = EPS) {
    return almostEqual(t1.diagonal(), t2.diagonal(), eps) &&
           almostEqual(t1.offDiagonal(), t2.offDiagonal(), eps);
}

/// Arbitrary norm of the tensor.
/// \todo Use some well-defined norm instead? (spectral norm, L1 or L2 norm, ...)
template <>
INLINE Float norm(const Tensor& t) {
    const Vector v = max(t.diagonal(), t.offDiagonal());
    ASSERT(isReal(v));
    return norm(v);
}

/// Arbitrary squared norm of the tensor
template <>
INLINE Float normSqr(const Tensor& t) {
    const Vector v = max(t.diagonal(), t.offDiagonal());
    return normSqr(v);
}

/// Returns the tensor of absolute values
template <>
INLINE auto abs(const Tensor& t) {
    return Tensor(abs(t.diagonal()), abs(t.offDiagonal()));
}

/// Returns the minimal element of the tensor.
template <>
INLINE Float minElement(const Tensor& t) {
    return min(minElement(t.diagonal()), minElement(t.offDiagonal()));
}

/// Component-wise minimum of two tensors.
template <>
INLINE Tensor min(const Tensor& t1, const Tensor& t2) {
    return Tensor(min(t1.diagonal(), t2.diagonal()), min(t1.offDiagonal(), t2.offDiagonal()));
}

/// Component-wise maximum of two tensors.
template <>
INLINE Tensor max(const Tensor& t1, const Tensor& t2) {
    return Tensor(max(t1.diagonal(), t2.diagonal()), max(t1.offDiagonal(), t2.offDiagonal()));
}

/// Clamping all components by range.
template <>
INLINE Tensor clamp(const Tensor& t, const Range& range) {
    return Tensor(clamp(t.diagonal(), range), clamp(t.offDiagonal(), range));
}

template <>
INLINE bool isReal(const Tensor& t) {
    return isReal(t.diagonal()) && isReal(t.offDiagonal());
}

template <>
INLINE auto less(const Tensor& t1, const Tensor& t2) {
    return Tensor(less(t1.diagonal(), t2.diagonal()), less(t1.offDiagonal(), t2.offDiagonal()));
}

template <>
INLINE Array<Float> getComponents(const Tensor& t) {
    return { t(0, 0), t(1, 1), t(2, 2), t(0, 1), t(0, 2), t(1, 2) };
}

/// Double-dot product t1 : t2 = sum_ij t1_ij t2_ij
INLINE Float ddot(const Tensor& t1, const Tensor& t2) {
    return dot(t1.diagonal(), t2.diagonal()) + 2._f * dot(t1.offDiagonal(), t2.offDiagonal());
}

/// SYMMETRIZED outer product of two vectors (simple outer product is not necessarily symmetric matrix).
INLINE Tensor outer(const Vector& v1, const Vector& v2) {
    /// \todo optimize
    return Tensor(v1 * v2,
        0.5_f * Vector(v1[0] * v2[1] + v1[1] * v2[0],
                    v1[0] * v2[2] + v1[2] * v2[0],
                    v1[1] * v2[2] + v1[2] * v2[1]));
}

/// Returns three eigenvalue of symmetric matrix.
INLINE StaticArray<Float, 3> findEigenvalues(const Tensor& t) {
    const Float n = norm(t);
    if (n < 1.e-12_f) {
        return { 0._f, 0._f, 0._f };
    }
    const Float p = -t.invariant<1>() / n;
    const Float q = -t.invariant<2>() / sqr(n);
    const Float r = -t.invariant<3>() / pow<3>(n);

    const Float a = q - p * p / 3._f;
    const Float b = (2._f * pow<3>(p) - 9._f * p * q + 27._f * r) / 27._f;
    const Float aCub = pow<3>(a) / 27._f;
    if (0.25_f * b * b + aCub >= 0._f) {
        return { 0._f, 0._f, 0._f };
    }
    ASSERT(a < 0._f);
    const Float t1 = 2._f * sqrt(-a / 3._f);
    const Float phi = acos(-0.5_f * b / sqrt(-aCub));
    const Vector v(phi / 3._f, (phi + 2 * PI) / 3._f, (phi + 4 * PI) / 3._f);
    const Vector sig = t1 * cos(v) - Vector(p / 3._f);
    return { sig[0] * n, sig[1] * n, sig[2] * n };
}

NAMESPACE_SPH_END
