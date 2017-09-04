#pragma once

/// \file QuantityIds.h
/// \brief Quantity identifiers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "QuantityHelpers.h"
#include <string>

NAMESPACE_SPH_BEGIN

/// Unique ID of basic quantities of SPH particles
enum class QuantityId {
    /// \name Common quantities
    ///@{
    POSITIONS,   ///< Positions (velocities, accelerations) of particles, always a vector quantity,
    MASSES,      ///< Paricles masses, always a scalar quantity.
    PRESSURE,    ///< Pressure, not affected by yielding or fragmentation model, always a scalar quantity.
    DENSITY,     ///< Density, always a scalar quantity.
    ENERGY,      ///< Specific internal energy, always a scalar quantity.
    SOUND_SPEED, ///< Sound speed, always a scalar quantity.
    DEVIATORIC_STRESS, ///< Deviatoric stress tensor, always a traceless tensor.
    SPECIFIC_ENTROPY,  ///< Specific entropy, always a scalar quantity.
    ///@}

    /// \name Density-independent SPH formulation
    ///@{
    ENERGY_DENSITY,      ///< Energy density
    ENERGY_PER_PARTICLE, ///< Internal energy per particle (analogy of particle masses)
    ///@}

    /// \name Damage and fragmentation model (see Benz & Asphaug, 1994)
    ///@{
    DAMAGE,              ///< Damage
    EPS_MIN,             ///< Activation strait rate
    M_ZERO,              ///< Coefficient M_0 of the stretched Weibull distribution
    EXPLICIT_GROWTH,     ///< Explicit growth of fractures
    N_FLAWS,             ///< Number of explicit flaws per particle
    FLAW_ACTIVATION_IDX, ///< Explicitly specified activation 'index' between 0 and N_particles. Lower value
                         /// mean lower activation strain rate of a flaw. Used only for testing purposes, by
                         /// default activation strain rates are automatically computed from Weibull
                         /// distribution.
    STRESS_REDUCING,     ///< Total stress reduction factor due to damage and yielding. Is always scalar.
    ///@}

    /// \name N-body & Solid sphere physics
    ///@{
    GRAVITY_POTENTIAL, ///< Gravitational potential; not needed for solution, but needs to be included to the
                       /// total energy of the system, otherwise the energy will not be conserved.
    ANGULAR_VELOCITY,  ///< Angular velocity of particles (spheres). Note that SPH particles have no angular
                       /// velocity.
    MOMENT_OF_INERTIA, ///< Moment of inertia of particles, analogy of particle masses for rotation
    ///@}

    /// \name Stress-strain analysis
    ///@{
    DISPLACEMENT, ///< Displacement vector, a solution of the stress analysis
    ///@}

    /// \name Intermediate quantities
    ///@{
    VELOCITY_GRADIENT,           ///< Velocity gradient
    VELOCITY_DIVERGENCE,         ///< Velocity divergence
    VELOCITY_ROTATION,           ///< Velocity rotation
    ROTATION_RATE,               ///< Rotation rate tensor
    STRENGTH_VELOCITY_GRADIENT,  ///< Gradient computed by summing up particles with non-zero stress tensor
                                 /// from the same body and strengthless particles (from any body). Used to
                                 /// implement the 'spring interaction'.
    ANGULAR_MOMENTUM_CORRECTION, ///< Correction tensor used to improve conservation of total angular
                                 /// momentum.
    ///@}

    /// \name Artificial velocity
    ///@{
    AV_ALPHA,                     ///< Coefficient alpha of the artificial viscosity
    AV_BETA,                      ///< Coefficient beta of the artificial viscosity
    AV_BALSARA,                   ///< Balsara factor
    AV_STRESS,                    ///< Artificial stress by Monaghan \cite Monaghan_1999
    INTERPARTICLE_SPACING_KERNEL, ///< Auxiliary quantity needed for evaluating artificial stress
    ///@}

    /// \name SPH modifications & corrections
    ///@{
    GRAD_H, ///< Grad-h terms, appear in self-consistent derivation of SPH equatios due to non-uniform
            /// smoothing lenghts.
    XSPH_VELOCITIES, ///< XSPH corrections to velocity. Only modifies evolution equation for particle
                     /// position, velocity (as an input for velocity divergence, ...) is NOT affected.
    ///@}

    /// \name SPH statistics
    ///@{
    NEIGHBOUR_CNT,  ///< Number of neighbouring particles (in radius h * kernel.radius)
    SURFACE_NORMAL, ///< Vector approximating surface normal
    ///@}

    /// \name Particle flags & Materials
    ///@{
    FLAG,             ///< ID of original body, used to implement discontinuities between bodies in SPH
    INITIAL_POSITION, ///< Initial position of particles, kept constant during the run
    ///@}
};

/// Auxiliary information about quantity that aren't stored directly in \ref Quantity
struct QuantityMetadata {

    /// Full name of the quantity (i.e. 'Density', 'Deviatoric stress', ...)
    std::string quantityName;

    /// Name of the 1st derivative. Usually it's just quantityName + 'derivative', but not always (for example
    /// 'Velocity' instead of 'Position derivative')
    std::string derivativeName;

    /// Name of the second derivative. Usually it's just quantityName + '2nd derivative'
    std::string secondDerivativeName;

    /// Short designation of the quantiy (i.e. 'rho', 's', ...)
    std::string label;

    /// \todo Units & dimensional analysis !

    /// \brief Variable expectedType contains a type the quantity will most likely have.
    ///
    /// The code does not assign fixed types to quantities, i.e. it's possible to create a tensor quantity
    /// QuantityId::DENSITY. This allows to use different modifications of SPH (tensor artificial viscosity,
    /// etc.), even though most quantities have only one type in any meaningful SPH simulation (density will
    /// always be scalar, for example).
    ValueEnum expectedType;

    QuantityMetadata(const std::string& fullName,
        const std::string& label,
        const ValueEnum type,
        const std::string& derivativeName = "",
        const std::string& secondDerivativeName = "");
};

/// Returns the quantity information using quantity ID.
QuantityMetadata getMetadata(const QuantityId key);

/// Print full quantity name into the stream.
INLINE std::ostream& operator<<(std::ostream& stream, const QuantityId key) {
    stream << getMetadata(key).quantityName;
    return stream;
}

NAMESPACE_SPH_END
