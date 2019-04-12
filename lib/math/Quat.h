#pragma once

/// \file Quat.h
/// \brief Quaternion algebra
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "math/AffineMatrix.h"

NAMESPACE_SPH_BEGIN

/// \brief Quaternion representing an axis of rotation and a (half of) rotation angle
///
/// Convenient holder of any rotation, as it contains only 4 components and can only represent a rotation,
/// compared to matrix with 9 components, representing any linear transformation.
class Quat {
private:
    Vector v;

public:
    Quat() = default;

    /// \brief Creates a quaternion given rotation axis and angle of rotation.
    ///
    /// Axis does not have to be normalized.
    Quat(const Vector& axis, const Float angle) {
        ASSERT(getSqrLength(axis) > 0._f);
        Vector normAxis;
        Float length;
        tieToTuple(normAxis, length) = getNormalizedWithLength(axis);

        const Float s = sin(0.5_f * angle);
        const Float c = cos(0.5_f * angle);

        v = normAxis * s;
        v[3] = c;
    }

    /// \brief Creates a quaternion given a rotation matrix
    ///
    /// The matrix must be orthogonal with det(matrix) = 1
    explicit Quat(const AffineMatrix& m) {
        ASSERT(m.translation() == Vector(0._f));
        ASSERT(m.isOrthogonal());
        const Float w = 0.5_f * sqrt(1.f + m(0, 0) + m(1, 1) + m(2, 2));
        const Float n = 0.25_f / w;
        v[X] = (m(2, 1) - m(1, 2)) * n;
        v[Y] = (m(0, 2) - m(2, 0)) * n;
        v[Z] = (m(1, 0) - m(0, 1)) * n;
        v[H] = w;
    }

    /// \brief Returns the normalized rotational axis.
    INLINE Vector axis() const {
        ASSERT(v[H] != 1._f);
        return v / sqrt(1._f - sqr(v[H]));
    }

    /// \brief Returns the angle of rotation (in radians).
    INLINE Float angle() const {
        return acos(v[H]) * 2._f;
    }

    INLINE Float& operator[](const Size idx) {
        return v[idx];
    }

    INLINE Float operator[](const Size idx) const {
        return v[idx];
    }

    /// \brief Converts the quaternion into a rotation matrix.
    AffineMatrix convert() const {
        const Float n = getSqrLength(v) + sqr(v[3]);
        const Vector s = v * (n > 0._f ? 2._f / n : 0._f);
        const Vector w = s * v[3];

        const Float xx = v[X] * s[X];
        const Float xy = v[X] * s[Y];
        const Float xz = v[X] * s[Z];
        const Float yy = v[Y] * s[Y];
        const Float yz = v[Y] * s[Z];
        const Float zz = v[Z] * s[Z];

        return AffineMatrix( //
            Vector(1._f - yy - zz, xy - w[Z], xz + w[Y]),
            Vector(xy + w[Z], 1._f - xx - zz, yz - w[X]),
            Vector(xz - w[Y], yz + w[X], 1._f - xx - yy));
    }
};

NAMESPACE_SPH_END
