#pragma once

/// \file Sphere.h
/// \brief Object representing a three-dimensional sphere
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/geometry/Box.h"

NAMESPACE_SPH_BEGIN

enum class IntersectResult {
    BOX_INSIDE_SPHERE,  ///< Sphere contains the whole box
    BOX_OUTSIDE_SPHERE, ///< Sphere has no intersection with the box
    INTERESECTION,      ///< Sphere intersects the box
};

class Sphere {
private:
    /// Center
    Vector p;

    /// Radius
    Float r;

public:
    /// \brief Creates an uninitialized sphere.
    Sphere() = default;

    /// \brief Creates a sphere given its center and radius
    Sphere(const Vector& center, const Float radius)
        : p(center)
        , r(radius) {
        ASSERT(radius >= 0._f);
    }

    INLINE Vector center() const {
        return p;
    }

    INLINE Vector& center() {
        return p;
    }

    INLINE Float radius() const {
        return r;
    }

    INLINE Float& radius() {
        return r;
    }

    INLINE Float volume() const {
        return sphereVolume(r);
    }

    INLINE bool contains(const Vector& v) const {
        return getSqrLength(p - v) < sqr(r);
    }

    INLINE Box getBBox() const {
        return Box(p - Vector(r), p + Vector(r));
    }

    /// \brief Checks if the sphere intersects another sphere.
    ///
    /// If one sphere contains the other one entirely, it counts as an intersections.
    INLINE bool intersects(const Sphere& other) const {
        return getSqrLength(p - other.p) < sqr(r + other.r);
    }

    /// \brief Checks whether the sphere partially or fully overlaps given box.
    INLINE bool overlaps(const Box& box) const {
        const Vector leftOf = max(box.lower() - p, Vector(0._f));
        const Vector rightOf = max(p - box.upper(), Vector(0._f));
        const Float rSqr = sqr(r);
        const Float distSqr = rSqr - getSqrLength(leftOf) - getSqrLength(rightOf);
        return distSqr > 0._f;
    }

    /// \brief Checks the intersection of the sphere with a box
    /// \todo not all branches are actually needed by TreeWalk, possibly optimize by some constexpr flag
    INLINE IntersectResult intersectsBox(const Box& box) const {
        ASSERT(box != Box::EMPTY());
        if (!this->overlaps(box)) {
            return IntersectResult::BOX_OUTSIDE_SPHERE;
        }
        const Float rSqr = sqr(r);
        // either the whole box is inside the sphere, or it intersects the sphere
        auto vertexInsideSphere = [&](const Vector& v) { return getSqrLength(v - p) < rSqr; };
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
