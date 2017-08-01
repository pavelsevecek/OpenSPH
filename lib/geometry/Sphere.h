#pragma once

/// \file Sphere.h
/// \brief Object representing a three-dimensional sphere
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "geometry/Box.h"

NAMESPACE_SPH_BEGIN

enum class IntersectResult {
    SPHERE_CONTAINS_BOX, ///< Sphere contains the whole box
    BOX_CONTAINS_SPHERE, ///< Box contains the whole sphere
    NO_INTERSECTION,     ///< Sphere has no intersection with the box
    INTERESECTION,       ///< Sphere intersects the box
};

class Sphere {
private:
    /// Radius is stored as 4th component
    Vector centerAndRadius;

public:
    Sphere(const Vector& center, const Float radius)
        : centerAndRadius(center) {
        centerAndRadius[H] = radius;
    }

    INLINE Vector center() const {
        return centerAndRadius;
    }

    INLINE Float radius() const {
        return centerAndRadius[H];
    }

    INLINE Float volume() const {
        return sphereVolume(this->radius());
    }

    /// \brief Checks the intersection of the sphere with a box
    /// \todo not all branches are actually needed by TreeWalk, possibly optimize by some constexpr flag
    INLINE IntersectResult intersectsBox(const Box& box) const {
        const Vector leftOf = max(box.lower() - centerAndRadius, Vector(0._f));
        const Vector rightOf = max(centerAndRadius - box.upper(), Vector(0._f));
        const Float rSqr = sqr(this->radius());
        const Float distSqr = rSqr - getSqrLength(leftOf) - getSqrLength(rightOf);
        if (distSqr <= 0._f) {
            return IntersectResult::NO_INTERSECTION;
        } else if (distSqr == rSqr) {
            // sphere center is inside the box
            const Vector maxDist = max(centerAndRadius - box.lower(), box.upper() - centerAndRadius);
            const Vector minDist = min(centerAndRadius - box.lower(), box.upper() - centerAndRadius);
            // assert seems to fail due to floating point arithmetics
            // ASSERT(minElement(maxDist) >= 0._f && minElement(minDist) >= 0._f);
            const Float maxDistSqr = getSqrLength(maxDist);
            if (maxDistSqr < rSqr) {
                // box is entirely inside the sphere
                return IntersectResult::SPHERE_CONTAINS_BOX;
            } else {
                if (minElement(minDist) > this->radius()) {
                    // sphere is entirely inside the box
                    return IntersectResult::BOX_CONTAINS_SPHERE;
                } else {
                    return IntersectResult::INTERESECTION;
                }
            }
            return IntersectResult::SPHERE_CONTAINS_BOX;
        } else {
            // sphere must intersect the box
            return IntersectResult::INTERESECTION;
        }
    }
};


NAMESPACE_SPH_END
