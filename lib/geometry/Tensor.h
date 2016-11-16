#pragma once

/// Basic algebra for symmetric 2nd order tensors
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz


#include "geometry/Vector.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class Tensor : public Object {
private:
    Vector diag; // diagonal part
    Vector off;  // over/below diagonal

public:
    Tensor() = default;

    Tensor(const Tensor& other)
        : diag(other.diag)
        , off(other.off) {}

    /// Construct tensor given its diagonal vector and a vector of off-diagonal elements (sorted top-bottom
    /// and left-right).
    Tensor(const Vector& diag, const Vector& off)
        : diag(diag)
        , off(off) {}

    /// Construct tensor given three vectors as rows. Matrix represented by the vectors MUST be symmetric,
    /// checked by assert.
    Tensor(const Vector& v0, const Vector& v1, const Vector& v2) {
        ASSERT(v0[1] == v1[0]);
        ASSERT(v0[2] == v2[0]);
        ASSERT(v1[2] == v2[1]);
        diag = Vector(v0[0], v1[1], v2[2]);
        off  = Vector(v0[1], v0[2], v1[2]);
    }

    INLINE Tensor& operator=(const Tensor& other) {
        diag = other.diag;
        off  = other.off;
        return *this;
    }

    /// Returns a row of the matrix.
    INLINE Vector operator[](const int idx) const {
        ASSERT(unsigned(idx) < 3);
        switch (idx) {
        case 0:
            return Vector(diag[0], off[0], off[1]);
        case 1:
            return Vector(off[0], diag[1], off[2]);
        case 2:
            return Vector(off[1], off[2], diag[2]);
        default:
            throw std::exception();
        }
    }

    /// Returns a given element of the matrix.
    INLINE Float& operator()(const int rowIdx, const int colIdx) {
        if (rowIdx == colIdx) {
            // diagonal
            return diag[rowIdx];
        } else {
            return off[rowIdx + colIdx - 1];
        }
    }

    /// Returns the diagonal part of the tensor
    INLINE const Vector& diagonal() const { return diag; }

    /// Returns the off-diagonal elements of the tensor
    INLINE const Vector& offDiagonal() const { return off; }

    /// Applies the tensor on given vector
    INLINE Vector operator*(const Vector& v) const {
        /// \todo optimize using _mm_shuffle_ps
        return Vector(diag[0] * v[0] + off[0] * v[1] + off[1] * v[2],
                      off[0] * v[0] + diag[1] * v[1] + off[2] * v[2],
                      off[1] * v[0] + off[2] * v[1] + diag[2] * v[2]);
    }

    INLINE bool operator==(const Tensor& other) const { return diag == other.diag && off == other.off; }

    /// Returns an identity tensor.
    static Tensor identity() { return Tensor(Vector(1._f, 1._f, 1._f), Vector(0._f, 0._f, 0._f)); }

    /// Returns a tensor with all zeros.
    static Tensor null() { return Tensor(Vector(0._f), Vector(0._f)); }

    /// Returns the determinant of the tensor
    INLINE Float determinant() const {
        return diag[0] * diag[1] * diag[2] + 2 * off[0] * off[1] * off[2] -
               (dot(Math::sqr(off), Vector(diag[2], diag[1], diag[0])));
    }

    /// Return the trace of the tensor
    INLINE Float trace() const { return dot(diag, Vector(1._f)); }

    /// Returns n-th invariant of the tensor (1<=n<=3)
    template <int n>
    Float invariant() const {
        switch (n) {
        case 1:
            return trace();
        case 2:
            /// \todo optimize with shuffle
            return getSqrLength(off) - (diag[1] * diag[2] + diag[2] * diag[0] + diag[0] * diag[1]);
        case 3:
            return determinant();
        }
    }

    Tensor transpose() const {}

    Tensor inverse() const {
        const Float det = determinant();
        ASSERT(det != 0._f);
        Vector invDiag, invOff;
        /// \todo optimize using SSE
        invDiag[0] = diag[1] * diag[2] - Math::sqr(off[2]);
        invDiag[1] = diag[2] * diag[0] - Math::sqr(off[1]);
        invDiag[2] = diag[0] * diag[1] - Math::sqr(off[0]);
        invOff[0]  = off[1] * off[2] - diag[2] * off[0];
        invOff[1]  = off[2] * off[0] - diag[1] * off[1];
        invOff[2]  = off[0] * off[1] - diag[0] * off[2];
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
    friend TStream& operator<<(TStream& stream, const Tensor& v) {
        stream << v[0] << std::endl << v[1] << std::endl << v[2];
    }
};

namespace Math {
    /// Checks if two tensors are equal to some given accuracy.
    INLINE bool almostEqual(const Tensor& t1, const Tensor& t2, const Float eps = EPS) {
        return almostEqual(t1.diagonal(), t2.diagonal(), eps) &&
               almostEqual(t1.offDiagonal(), t2.offDiagonal(), eps);
    }

    /// Arbitrary norm of the tensor.
    INLINE Float norm(const Tensor& t) {
        const Vector v = Math::max(t.diagonal(), t.offDiagonal());
        return norm(v);
    }
}

INLINE StaticArray<Float, 3> findEigenvalues(const Tensor& t) {
    const Float n = 1._f;// Math::norm(t);
    const Float p = -t.invariant<1>() / n;
    const Float q = -t.invariant<2>() / Math::sqr(n);
    const Float r = -t.invariant<3>() / Math::pow<3>(n);

    const Float a = q - p * p / 3._f;
    ASSERT(a < 0._f);
    const Float b     = (2._f * Math::pow<3>(p) - 9._f * p * q + 27._f * r) / 27._f;
    const Float aCub  = Math::pow<3>(a) / 27._f;
    const Float roots = 0.25_f * b * b + aCub;
    ASSERT(roots < 0._f);
    const Float t1  = 2._f * Math::sqrt(-a / 3._f);
    const Float phi = Math::acos(-0.5_f * b / Math::sqrt(-aCub));
    const Vector v(phi / 3._f, (phi + 2 * Math::PI) / 3._f, (phi + 4 * Math::PI) / 3._f);
    const Vector sig = t1 * Math::cos(v) - Vector(p / 3._f);
    return { sig[0] * n, sig[1] * n, sig[2] *n};
}


NAMESPACE_SPH_END
