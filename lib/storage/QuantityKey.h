#pragma once

/// Storage of all quantities of SPH particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

/// Unique ID of basic quantities of SPH particles
enum class QuantityKey {
    /// Common quantities
    R,   ///< Positions (velocities, accelerations) of particles
    M,   ///< Paricles masses
    P,   ///< Pressure
    RHO, ///< Density
    U,   ///< Specific internal energy
    CS,  ///< Sound speed
    S,   ///< Deviatoric stress tensor
    A,   ///< Specific entropy

    /// Damage and fragmentation model (see Benz & Asphaug, 1994)
    D,       ///< Damage
    EPS,     ///< Activation strait rate
    M_ZERO,  ///< Coefficient M_0 of the stretched Weibull distribution
    DD_DT,   ///< Explicit growth of fractures
    N_FLAWS, ///< Number of explicit flaws per particle

    /// Artificial velocity
    DIVV,  ///< Velocity divergence
    ROTV,  ///< Velocity rotation
    ALPHA, ///< Coefficient alpha of the artificial viscosity
    BETA,  ///< Coefficient beta of the artificial viscosity

    /// Materials
    MAT_ID, ///< Material ID
};

/// \todo rewrite this to something less stupid
INLINE std::string getQuantityName(const QuantityKey key) {
    switch (key) {
    case QuantityKey::R:
        return "Position";
    case QuantityKey::M:
        return "Particle mass";
    case QuantityKey::P:
        return "Pressure";
    case QuantityKey::RHO:
        return "Density";
    case QuantityKey::U:
        return "Specific energy";
    case QuantityKey::CS:
        return "Sound speed";
    case QuantityKey::S:
        return "Deviatoric stress tensor";
    case QuantityKey::D:
        return "Damage";
    default:
        NOT_IMPLEMENTED;
    }
}

INLINE std::string getDerivativeName(const QuantityKey key) {
    switch (key) {
    case QuantityKey::R:
        return "Velocity";
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
