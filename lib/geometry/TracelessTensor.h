#pragma once

/// Symmetric traceless 2nd order tensor
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Tensor.h"

NAMESPACE_SPH_BEGIN

class TracelessTensor : public Object {
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

    TracelessTensor(const Vector& m, const Float m12)
        : m(m)
        , m12(m12) {}

public:
    TracelessTensor() = default;

    TracelessTensor(const TracelessTensor& other)
        : m(other.m)
        , m12(other.m12) {}

    /// Construct traceless tensor using other tensor (not traceless in general). "Tracelessness" of the
    /// tensor is checked by assert.
    TracelessTensor(const Tensor& other) {
        ASSERT(other.trace() < EPS * Math::norm(other));
        m                = other.diagonal();
        const Vector off = other.offDiagonal();
        m[M01]           = off[0];
        m[M02]           = off[1];
        m12              = off[2];
    }

    /// Initialize all components of the tensor to given value.
    TracelessTensor(const Float value)
        : m(value)
        , m12(value) {}

    /// Construct tensor given three vectors as rows. Matrix represented by the vectors MUST be symmetric and
    /// traceless, checked by assert.
    TracelessTensor(const Vector& v0, const Vector& v1, const Vector& v2) {
        ASSERT(v0[1] == v1[0]);
        ASSERT(v0[2] == v2[0]);
        ASSERT(v1[2] == v2[1]);
        ASSERT(v0[0] + v1[1] + v2[2] < EPS * (Math::norm(v1) + Math::norm(v1) + Math::norm(v2)));
        m   = Vector(v0[0], v1[1], v0[1], v0[2]);
        m12 = v1[2];
    }

    INLINE TracelessTensor& operator=(const TracelessTensor& other) {
        m   = other.m;
        m12 = other.m12;
        return *this;
    }

    /// Explicit conversion to ordinary Tensor
    INLINE explicit operator Tensor() const {
        return Tensor(Vector(m[M00], m[M11], 1._f - m[M00] - m[M11]), Vector(m[M01], m[M02], m12));
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
            return Vector(m[M02], m12, 1._f - m[M00] - m[M11]);
        default:
            throw std::exception();
        }
    }

    /// Returns a given element of the matrix.
    INLINE Float operator()(const int rowIdx, const int colIdx) {
        if (rowIdx == colIdx) {
            // diagonal
            if (rowIdx < 2) {
                return m[rowIdx];
            } else {
                return 1._f - m[0] - m[1];
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
                      m[M02] * v[0] + m12 * v[1] + (1._f - m[M00] - m[M11]) * v[2]);
    }

    /// Multiplies the tensor by a scalar
    INLINE friend TracelessTensor operator*(const TracelessTensor& t, const Float v) {
        return TracelessTensor(t.m * v, t.m12 * v);
    }

    INLINE friend TracelessTensor operator*(const Float v, const TracelessTensor& t) {
        return TracelessTensor(t.m * v, t.m12 * v);
    }

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

    INLINE bool operator==(const TracelessTensor& other) const { return m == other.m && m12 == other.m12; }


    /// Returns a tensor with all zeros.
    static TracelessTensor null() { return TracelessTensor(0._f); }

    Float maxElement() const {
        /// \todo optimize
        const Float vectorMax = Math::max(Math::max(m[0], m[1]), Math::max(m[2], m[3]));
        return Math::max(vectorMax, m12);
    }

    TracelessTensor clamp(const Range& range) const {
        return TracelessTensor(Math::clamp(m, range), Math::clamp(m12, range));
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const TracelessTensor& v) {
        stream << v[0] << std::endl << v[1] << std::endl << v[2];
        return stream;
    }
};

namespace Math {
    /*    /// Checks if two tensors are equal to some given accuracy.
        INLINE bool almostEqual(const TracelessTensor& t1, const TracelessTensor& t2, const Float eps = EPS) {
            return almostEqual(t1.diagonal(), t2.diagonal(), eps) &&
                   almostEqual(t1.offDiagonal(), t2.offDiagonal(), eps);
        }*/

    /// Arbitrary norm of the tensor.
    /// \todo Use some well-defined norm instead? (spectral norm, L1 or L2 norm, ...)
    /// \todo Same norm for Tensor and TracelessTensor
    INLINE Float norm(const TracelessTensor& t) { return Math::abs(t.maxElement()); }

    /// Arbitrary squared norm of the tensor
    INLINE Float normSqr(const TracelessTensor& t) { return Math::sqr(t.maxElement()); }

    /// Clamping all components by range.
    template<>
    INLINE TracelessTensor clamp(const TracelessTensor& t, const Range& range) { return t.clamp(range); }
}


/// Double-dot product t1 : t2 = sum_ij t1_ij t2_ij
INLINE Float ddot(const TracelessTensor& UNUSED(t1), const Tensor& UNUSED(t2)) {
    NOT_IMPLEMENTED
}

INLINE Float ddot(const Tensor& UNUSED(t1), const TracelessTensor& UNUSED(t2)) {
    NOT_IMPLEMENTED
}

INLINE Float ddot(const TracelessTensor& UNUSED(t1), const TracelessTensor& UNUSED(t2)) {
    NOT_IMPLEMENTED
}

NAMESPACE_SPH_END
