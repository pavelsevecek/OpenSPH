#pragma once

#include "quantities/Attractor.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

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
