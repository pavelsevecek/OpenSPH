#pragma once

/// \file Sphere.h
/// \brief Object representing a three-dimensional sphere
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "geometry/Box.h"

NAMESPACE_SPH_BEGIN

enum class IntersectResult {
    BOX_INSIDE_SPHERE,  ///< Sphere contains the whole box
    BOX_OUTSIDE_SPHERE, ///< Sphere has no intersection with the box
    INTERESECTION,      ///< Sphere intersects the box
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
        ASSERT(box != Box::EMPTY());
        const Vector leftOf = max(box.lower() - centerAndRadius, Vector(0._f));
        const Vector rightOf = max(centerAndRadius - box.upper(), Vector(0._f));
        const Float rSqr = sqr(this->radius());
        const Float distSqr = rSqr - getSqrLength(leftOf) - getSqrLength(rightOf);
        if (distSqr <= 0._f) {
            return IntersectResult::BOX_OUTSIDE_SPHERE;
        }
        // either the whole box is inside the sphere, or it intersects the sphere
        auto vertexInsideSphere = [&](const Vector& v) { return getSqrLength(v - centerAndRadius) < rSqr; };
        if (!vertexInsideSphere(box.lower())) {
            return IntersectResult::INTERESECTION;
        }
        if (!vertexInsideSphere(box.lower() + Vector(box.size()[X], 0._f, 0._f))) {
            return IntersectResult::INTERESECTION;
        }
        if (!vertexInsideSphere(box.lower() + Vector(0._f, box.size()[Y], 0._f))) {
            return IntersectResult::INTERESECTION;
        }
        if (!vertexInsideSphere(box.lower() + Vector(0._f, 0._f, box.size()[Z]))) {
            return IntersectResult::INTERESECTION;
        }
        if (!vertexInsideSphere(box.upper())) {
            return IntersectResult::INTERESECTION;
        }
        if (!vertexInsideSphere(box.upper() - Vector(box.size()[X], 0._f, 0._f))) {
            return IntersectResult::INTERESECTION;
        }
        if (!vertexInsideSphere(box.upper() - Vector(0._f, box.size()[Y], 0._f))) {
            return IntersectResult::INTERESECTION;
        }
        if (!vertexInsideSphere(box.upper() - Vector(0._f, 0._f, box.size()[Z]))) {
            return IntersectResult::INTERESECTION;
        }
        return IntersectResult::BOX_INSIDE_SPHERE;
    }
};


NAMESPACE_SPH_END
