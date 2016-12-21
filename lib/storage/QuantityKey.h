#pragma once

/// Storage of all quantities of SPH particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

/// Unique ID of basic quantities of SPH particles
enum class QuantityKey {
    R        = 0,  ///< Positions (velocities, accelerations) of particles
    M        = 1,  ///< Paricles masses
    P        = 2,  ///< Pressure
    RHO      = 3,  ///< Density
    U        = 4,  ///< Specific internal energy
    CS       = 5,  ///< Sound speed
    S        = 6,  ///< Deviatoric stress tensor
    A        = 7,  ///< Specific entropy
    D        = 8,  ///< Damage
    DIVV     = 9,  ///< Velocity divergence
    ROTV     = 10, ///< Velocity rotation
    ALPHA    = 11, ///< Coefficient alpha of the artificial viscosity
    BETA     = 12, ///< Coefficient beta of the artificial viscosity
    MAT_ID   = 13, ///< Material ID
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
