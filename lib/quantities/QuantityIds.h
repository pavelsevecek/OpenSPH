#pragma once

/// Storage of all quantities of SPH particles.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "common/Assert.h"
#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

/// Unique ID of basic quantities of SPH particles
enum class QuantityId {
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
    YIELDING_REDUCE,

    /// Intermediate quantities
    VELOCITY_GRADIENT,           ///< Velocity gradient
    VELOCITY_DIVERGENCE,         ///< Velocity divergence
    VELOCITY_ROTATION,           ///< Velocity rotation
    STRENGTH_VELOCITY_GRADIENT,  ///< Gradient computed by summing up particles with non-zero stress tensor
                                 /// from the same body and strengthless particles (from any body). Used to
                                 /// implement the 'spring interaction'.
    ANGULAR_MOMENTUM_CORRECTION, ///< Correction tensor used to improve conservation of total angular
                                 /// momentum.

    /// Artificial velocity
    AV_ALPHA, ///< Coefficient alpha of the artificial viscosity
    AV_BETA,  ///< Coefficient beta of the artificial viscosity

    /// SPH statistics
    NEIGHBOUR_CNT, ///< Number of neighbouring particles (in radius h * kernel.radius)

    /// Particle flags & Materials
    FLAG,         ///< ID of original body, used to implement discontinuities between bodies in SPH
    MATERIAL_IDX, ///< Material ID
};

INLINE std::string getQuantityName(const QuantityId key) {
    switch (key) {
    case QuantityId::POSITIONS:
        return "Position";
    case QuantityId::MASSES:
        return "Particle mass";
    case QuantityId::PRESSURE:
        return "Pressure";
    case QuantityId::DENSITY:
        return "Density";
    case QuantityId::ENERGY:
        return "Spec. energy";
    case QuantityId::SOUND_SPEED:
        return "Sound speed";
    case QuantityId::DEVIATORIC_STRESS:
        return "Stress";
    case QuantityId::DAMAGE:
        return "Damage";
    case QuantityId::NEIGHBOUR_CNT:
        return "Neigh. cnt";
    case QuantityId::MATERIAL_IDX:
        return "Mat. idx";
    case QuantityId::FLAG:
        return "Flag";
    default:
        NOT_IMPLEMENTED;
    }
}

template <typename TStream>
INLINE TStream& operator<<(TStream& stream, const QuantityId key) {
    stream << getQuantityName(key);
    return stream;
}

INLINE std::string getDerivativeName(const QuantityId key) {
    switch (key) {
    case QuantityId::POSITIONS:
        return "Velocity";
    case QuantityId::DEVIATORIC_STRESS:
        return "[debug] dS/dt";
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
