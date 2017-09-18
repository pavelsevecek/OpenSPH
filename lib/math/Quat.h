#pragma once

/// \file Quat.h
/// \brief Quaternion algebra
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

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

    Float& operator[](const Size idx) {
        return v[idx];
    }

    Float operator[](const Size idx) const {
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
