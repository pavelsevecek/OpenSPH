#pragma once

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \brief Computes the total mass of the storage.
Float getTotalMass(const Storage& storage);

Vector getTotalMomentum(const Storage& storage);

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
        p.position() = func(p.position());
    }
}

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
        p.position() = posFunc(p.position());
        p.velocity() = velFunc(p.velocity());
    }
}

/// \brief Changes the inertial system by given offset of positions and velocities.
void moveInertialFrame(Storage& storage, const Vector positionOffset, const Vector velocityOffset);

/// \brief Modifies particle positions so that their center of mass lies at the origin.
///
/// Function can be also used for particle velocities, modifying them so that the total momentum is zero.
/// \param m Particle masses; must be positive values
/// \param r Particle positions (or velocities)
/// \return Computed center of mass, subtracted from positions.
Vector moveToCenterOfMassFrame(ArrayView<const Float> m, ArrayView<Vector> r);

/// \brief Modifies particle positions and velocities so that the center of mass is at the origin and the
/// total momentum is zero.
void moveToCenterOfMassFrame(Storage& storage);

NAMESPACE_SPH_END
