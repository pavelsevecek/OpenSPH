#pragma once

/// \file Plane.h
/// \brief Infinite plane definited by its general equation.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/geometry/Triangle.h"

NAMESPACE_SPH_BEGIN

/// \brief Represents a plane in 3D space.
class Plane {
private:
    Vector v;

public:
    /// \brief Creates the plane using its normal and a point lying in the plane.
    ///
    /// The normal has to be normalized.
    Plane(const Vector& p, const Vector& n) {
        SPH_ASSERT(almostEqual(getLength(n), 1._f), n, getLength(n));
        v = n;
        v[3] = -dot(p, n);
    }

    /// \brief Creates the plane from three vertices of a triangle
    Plane(const Triangle& tri)
        : Plane(tri[0], getNormalized(tri.normal())) {}

    /// \brief Returns the normal of the plane.
    const Vector& normal() const {
        return v;
    }

    /// \brief Returns the signed distance of the point from the plane.
    Float signedDistance(const Vector& p) const {
        return dot(v, p) + v[3];
    }

    /// \brief Checks if the point lies above the plane.
    bool above(const Vector& p) const {
        return this->signedDistance(p) > 0._f;
    }

    /// \brief Returns the projection of the point to the plane.
    Vector project(const Vector& p) const {
        return p - v * dot(v, p);
    }

    /// \brief Finds the intersection with a line, given by its origin and direction.
    Vector intersection(const Vector& origin, const Vector& dir) const {
        SPH_ASSERT(dot(dir, normal()) != 0._f);
        const Float t = -signedDistance(origin) / dot(dir, this->normal());
        return origin + t * dir;
    }
};

NAMESPACE_SPH_END
