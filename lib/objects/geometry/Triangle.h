#pragma once

/// \brief Triangle.h
/// \brief Object representing a three-dimensional triangle
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/geometry/Box.h"

NAMESPACE_SPH_BEGIN

/// \brief Represents three vertices of the triangle
class Triangle {
private:
    Vector v[3];

public:
    Triangle() = default;

    Triangle(const Vector& v1, const Vector& v2, const Vector& v3)
        : v{ v1, v2, v3 } {
        ASSERT(isValid());
    }

    INLINE Vector& operator[](const Size idx) {
        return v[idx];
    }

    INLINE const Vector& operator[](const Size idx) const {
        return v[idx];
    }

    INLINE Vector center() const {
        return (v[0] + v[1] + v[2]) / 3._f;
    }

    INLINE Vector normal() const {
        ASSERT(this->isValid());
        const Vector v12 = v[2] - v[1];
        const Vector v02 = v[2] - v[0];
        return getNormalized(cross(v12, v02));
    }

    INLINE Box getBBox() const {
        Box box;
        for (Size i = 0; i < 3; ++i) {
            box.extend(v[i]);
        }
        return box;
    }

    INLINE bool isValid() const {
        if (!isReal(v[0]) || !isReal(v[1]) || !isReal(v[2])) {
            return false;
        }
        const Vector v12 = v[2] - v[1];
        const Vector v02 = v[2] - v[0];
        return sqr(dot(v12, v02)) < (1._f - EPS) * getSqrLength(v12) * getSqrLength(v02);
    }
};

NAMESPACE_SPH_END