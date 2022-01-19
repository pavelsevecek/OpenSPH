#pragma once

#include "quantities/Attractor.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \brief Convenience function to get the bounding box of all particles.
///
/// This takes into account particle radii, using given kernel radius.
// Box getBoundingBox(const Storage& storage, const Float radius = 2._f);

/// \brief Returns the center of mass of all particles.
///
/// Function can be called even if the storage does not store particle masses, in which case all particles are
/// assumed to have equal mass.
// Vector getCenterOfMass(const Storage& storage);

/// \brief Computes the total mass of the storage.
Float getTotalMass(const Storage& storage);

/// \brief Computes the total momentum of all particles and attractors.
Vector getTotalMomentum(const Storage& storage);

/// \brief Changes the inertial system by given offset of positions and velocities.
void moveInertialFrame(Storage& storage, const Vector& positionOffset, const Vector& velocityOffset);

/// \brief Modifies particle positions and velocities so that the center of mass is at the origin and the
/// total momentum is zero.
void moveToCenterOfMassFrame(Storage& storage);

/// \brief Provides generic transform of positions.
template <typename TPositionFunc>
void transform(Storage& storage, const TPositionFunc& func) {
    if (!storage.empty()) {
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r.size(); ++i) {
            const Vector r_new = func(r[i]);
            // preserve H
            r[i] = setH(r_new, r[i][H]);
        }
    }
    for (Attractor& p : storage.getAttractors()) {
        p.position = func(p.position);
    }
}

/// \brief Provides generic transform of positions and velocities.
template <typename TPositionFunc, typename TVelocityFunc>
void transform(Storage& storage, const TPositionFunc& posFunc, const TVelocityFunc& velFunc) {
    if (!storage.empty()) {
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        for (Size i = 0; i < r.size(); ++i) {
            const Vector r_new = posFunc(r[i]);
            const Vector v_new = velFunc(v[i]);
            // preserve H
            r[i] = setH(r_new, r[i][H]);
            v[i] = clearH(v_new);
        }
    }
    for (Attractor& p : storage.getAttractors()) {
        p.position = posFunc(p.position);
        p.velocity = velFunc(p.velocity);
    }
}

NAMESPACE_SPH_END
