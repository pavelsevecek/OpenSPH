#pragma once

/// Storage of all quantities of SPH particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

/// Unique ID of basic quantities of SPH particles
enum class QuantityKey {
    /// Common quantities
    POSITIONS,   ///< Positions (velocities, accelerations) of particles, always a vector quantity,
    MASSES,      ///< Paricles masses, always a scalar quantity.
    PRESSURE,    ///< Pressure, not affected by yielding or fragmentation model, always a scalar quantity.
    DENSITY,     ///< Density, always a scalar quantity.
    ENERGY,      ///< Specific internal energy, always a scalar quantity.
    SOUND_SPEED, ///< Sound speed, always a scalar quantity.
    DEVIATORIC_STRESS, ///< Deviatoric stress tensor, always a traceless tensor.
    SPECIFIC_ENTROPY,  ///< Specific entropy, always a scalar quantity.

    /// Density-independent SPH formulation
    ENERGY_DENSITY,      ///< Energy density
    ENERGY_PER_PARTICLE, ///< Internal energy per particle (analogy of particle masses)

    /// Damage and fragmentation model (see Benz & Asphaug, 1994)
    DAMAGE,              ///< Damage
    EPS_MIN,             ///< Activation strait rate
    M_ZERO,              ///< Coefficient M_0 of the stretched Weibull distribution
    EXPLICIT_GROWTH,     ///< Explicit growth of fractures
    N_FLAWS,             ///< Number of explicit flaws per particle
    FLAW_ACTIVATION_IDX, ///< Explicitly specified activation 'index' between 0 and N_particles. Lower value
                         /// mean lower activation strain rate of a flaw. Used only for testing purposes, by
                         /// default activation strain rates are automatically computed from Weibull
                         /// distribution.

    /// Artificial velocity
    VELOCITY_DIVERGENCE, ///< Velocity divergence
    VELOCITY_ROTATION,   ///< Velocity rotation
    AV_ALPHA,            ///< Coefficient alpha of the artificial viscosity
    AV_BETA,             ///< Coefficient beta of the artificial viscosity

    /// Materials
    MATERIAL_IDX, ///< Material ID
};

/// \todo rewrite this to something less stupid
INLINE std::string getQuantityName(const QuantityKey key) {
    switch (key) {
    case QuantityKey::POSITIONS:
        return "Position";
    case QuantityKey::MASSES:
        return "Particle mass";
    case QuantityKey::PRESSURE:
        return "Pressure";
    case QuantityKey::DENSITY:
        return "Density";
    case QuantityKey::ENERGY:
        return "Specific energy";
    case QuantityKey::SOUND_SPEED:
        return "Sound speed";
    case QuantityKey::DEVIATORIC_STRESS:
        return "Deviatoric stress";
    case QuantityKey::DAMAGE:
        return "Damage";
    default:
        NOT_IMPLEMENTED;
    }
}

INLINE std::string getDerivativeName(const QuantityKey key) {
    switch (key) {
    case QuantityKey::POSITIONS:
        return "Velocity";
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
