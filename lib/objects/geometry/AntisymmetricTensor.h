#pragma once

/// \file AntisymmetricTensor.h
/// \brief Basic algebra for antisymmetric 2nd order tensors
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017


#include "objects/geometry/SymmetricTensor.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN

struct PseudoVectorTag {};

const PseudoVectorTag PSEUDOVECTOR;

class AntisymmetricTensor {
private:
    Vector u;

public:
    AntisymmetricTensor() = default;

    /// Constructs an antisymmetric tensor given three independed components. Components x, y, z in vector
    /// correspond to components xy, xz, yz of the antisymmetric tensor.
    explicit AntisymmetricTensor(const Vector& v)
        : u(v) {}

    /// Constructs an antisymmetric tensor from a corresponding pseudovector. Here we use right-hand
    /// convention, same as for cross product.
    explicit AntisymmetricTensor(const PseudoVectorTag, const Vector& v)
        : u(v[Z], -v[Y], v[X]) {}

    /// Constructs an antisymmetric tensor by setting all components above the diagonal to the same value.
    explicit AntisymmetricTensor(const Float v)
        : u(v) {}

    /// Returns the components above diagonal as vector
    INLINE Vector& components() {
        return u;
    }

    /// Returns the components above diagonal as vector
    INLINE const Vector& components() const {
        return u;
    }

    /// Returns the associated pseudovector.
    Vector pseudovector() const {
        return Vector(u[Z], -u[Y], u[X]);
    }

    /// Returns a row of the matrix
    INLINE Vector operator[](const Size idx) const {
        switch (idx) {
        case 0:
            return Vector(0._f, u[X], u[Y]);
        case 1:
            return Vector(-u[X], 0._f, u[Z]);
        case 2:
            return Vector(-u[Y], -u[Z], 0._f);
        default:
            STOP;
        }
    }

    /// Returns element on given with given indices.
    INLINE Float operator()(const Size i, const Size j) const {
        if (i == j) {
            return 0._f;
        } else if (i < j) {
            return u[i + j - 1];
        } else {
            return -u[i + j - 1];
        }
    }

    INLINE static AntisymmetricTensor null() {
        return AntisymmetricTensor(Vector(0._f));
    }

    INLINE AntisymmetricTensor operator+=(const AntisymmetricTensor& other) {
        u += other.u;
        return *this;
    }

    INLINE AntisymmetricTensor operator-=(const AntisymmetricTensor& other) {
        u -= other.u;
        return *this;
    }

    INLINE AntisymmetricTensor operator-() const {
        return AntisymmetricTensor(-u);
    }

    friend bool operator==(const AntisymmetricTensor& t1, const AntisymmetricTensor& t2) {
        return t1.u == t2.u;
    }

    friend AntisymmetricTensor operator+(const AntisymmetricTensor& t1, const AntisymmetricTensor& t2) {
        return AntisymmetricTensor(t1.u + t2.u);
    }

    friend AntisymmetricTensor operator-(const AntisymmetricTensor& t1, const AntisymmetricTensor& t2) {
        return AntisymmetricTensor(t1.u - t2.u);
    }

    friend AntisymmetricTensor operator*(const AntisymmetricTensor& t, const Float v) {
        return AntisymmetricTensor(t.u * v);
    }

    friend AntisymmetricTensor operator*(const Float v, const AntisymmetricTensor& t) {
        return AntisymmetricTensor(t.u * v);
    }

    friend AntisymmetricTensor operator/(const AntisymmetricTensor& t, const Float v) {
        return AntisymmetricTensor(t.u / v);
    }

    friend std::ostream& operator<<(std::ostream& stream, const AntisymmetricTensor& t) {
        stream << std::setprecision(6) << std::setw(20) << t.u[X] << std::setw(20) << t.u[Y] << std::setw(20)
               << t.u[Z];
        return stream;
    }
};

/// ANTISYMMETRIZED outer product of two vectors (simple outer product is not necessarily antisymmetric
/// matrix).
INLINE AntisymmetricTensor antisymmetricOuter(const Vector& v1, const Vector& v2) {
    /// \todo optimize
    return AntisymmetricTensor(
        0.5_f *
        Vector(v1[X] * v2[Y] - v1[Y] * v2[X], v1[X] * v2[Z] - v1[Z] * v2[X], v1[Y] * v2[Z] - v1[Z] * v2[Y]));
}


/// Checks if two tensors are equal to some given accuracy.
INLINE bool almostEqual(const AntisymmetricTensor& t1, const AntisymmetricTensor& t2, const Float eps = EPS) {
    return almostEqual(t1.components(), t2.components(), eps);
}

/// Arbitrary norm of the tensor.
/// \todo Use some well-defined norm instead? (spectral norm, L1 or L2 norm, ...)
template <>
INLINE Float norm(const AntisymmetricTensor& t) {
    return norm(t.components());
}

/// Arbitrary squared norm of the tensor
template <>
INLINE Float normSqr(const AntisymmetricTensor& t) {
    return normSqr(t.components());
}

/// Returns the tensor of absolute values. Resulting tensor is necessarily symmetric
template <>
INLINE auto abs(const AntisymmetricTensor& t) {
    return SymmetricTensor(Vector(0._f), abs(t.components()));
}

/// Returns the minimal element of the tensor.
template <>
INLINE Float minElement(const AntisymmetricTensor& t) {
    return min(minElement(t.components()), minElement(-t.components()));
}

/// Component-wise minimum of two tensors.
template <>
INLINE AntisymmetricTensor min(const AntisymmetricTensor& t1, const AntisymmetricTensor& t2) {
    return AntisymmetricTensor(min(t1.components(), t2.components()));
}

/// Component-wise maximum of two tensors.
template <>
INLINE AntisymmetricTensor max(const AntisymmetricTensor& t1, const AntisymmetricTensor& t2) {
    return AntisymmetricTensor(max(t1.components(), t2.components()));
}

/// Clamping all components by range.
template <>
INLINE AntisymmetricTensor clamp(const AntisymmetricTensor& t, const Interval& range) {
    ASSERT(range.contains(0._f));
    const Float upper = max(-range.lower(), range.upper());
    return AntisymmetricTensor(clamp(t.components(), Interval(-upper, upper)));
}

template <>
INLINE bool isReal(const AntisymmetricTensor& t) {
    return isReal(t.components());
}

template <>
INLINE auto less(const AntisymmetricTensor& t1, const AntisymmetricTensor& t2) {
    return AntisymmetricTensor(less(t1.components(), t2.components()));
}

/// Double-dot product t1 : t2 = sum_ij t1_ij t2_ij
INLINE Float ddot(const AntisymmetricTensor& t1, const AntisymmetricTensor& t2) {
    return 2._f * dot(t1.components(), t2.components());
}


NAMESPACE_SPH_END
