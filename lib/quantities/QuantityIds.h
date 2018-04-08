#pragma once

/// \file QuantityIds.h
/// \brief Quantity identifiers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "QuantityHelpers.h"
#include <string>

NAMESPACE_SPH_BEGIN

/// \brief Unique IDs of basic quantities of SPH particles.
///
/// New quantities should be added to the BOTTOM of the enum in order to allow opening older binary files
/// (using \ref BinaryOutput).
enum class QuantityId {
    /// \name Common quantities
    ///@{

    /// Positions (velocities, accelerations) of particles, always a vector quantity,
    POSITION,

    /// Paricles masses, always a scalar quantity.
    MASS,

    /// Pressure, not affected by yielding or fragmentation model, always a scalar quantity.
    PRESSURE,

    /// Density, always a scalar quantity.
    DENSITY,

    /// Specific internal energy, always a scalar quantity.
    ENERGY,

    /// Sound speed, always a scalar quantity.
    SOUND_SPEED,

    /// Deviatoric stress tensor, always a traceless tensor.
    DEVIATORIC_STRESS,

    /// Specific entropy, always a scalar quantity.
    SPECIFIC_ENTROPY,

    ///@}

    /// \name Density-independent SPH formulation
    ///@{

    /// Energy density (energy per unit volume)
    ENERGY_DENSITY,

    /// Internal energy per particle (analogy of particle masses in DISPH)
    ENERGY_PER_PARTICLE,

    ///@}

    /// \name Damage and fragmentation model (see Benz & Asphaug, 1994)
    ///@{

    /// Damage
    DAMAGE,

    /// Activation strait rate
    EPS_MIN,

    /// Coefficient M_0 of the stretched Weibull distribution
    M_ZERO,

    /// Explicit growth of fractures
    EXPLICIT_GROWTH,

    /// Number of explicit flaws per particle
    N_FLAWS,

    /// Explicitly specified activation 'index' between 0 and N_particles. Lower value mean lower activation
    /// strain rate of a flaw. Used only for testing purposes, by default activation strain rates are
    /// automatically computed from Weibull distribution.
    FLAW_ACTIVATION_IDX,

    /// Total stress reduction factor due to damage and yielding. Is always scalar.
    STRESS_REDUCING,

    /// \todo
    MOHR_COULOMB_STRESS,

    /// \todo
    FRICTION_ANGLE,

    ///@}

    /// \name N-body & Solid sphere physics
    ///@{

    /// Gravitational potential; not needed for solution, but needs to be included to the total energy of the
    /// system, otherwise the energy will not be conserved.
    GRAVITY_POTENTIAL,

    /// Angular velocity of particles. Note that SPH particles in standard formulation have no angular
    /// velocity, this quantity is used by solid sphere solvers or by SPH modifications that include particle
    /// rotation.
    ANGULAR_VELOCITY,

    /// Angular momentum of particles. Useful replacement of angular velocity quantity as angular momentum is
    /// always conserved.
    ANGULAR_MOMENTUM,

    /// Current rotation state of the particles. This is only needed for testing purposes, as SPH particles
    /// are spherically symmetric.
    PHASE_ANGLE,

    /// Moment of inertia of particles, analogy of particle masses for rotation
    MOMENT_OF_INERTIA,

    /// Local coordinates of a particle; moment of inertia is typically expressed in these coordinates.
    LOCAL_FRAME,

    ///@}

    /// \name Stress-strain analysis
    ///@{

    /// Displacement vector, a solution of the stress analysis
    DISPLACEMENT,

    ///@}

    /// \name Intermediate quantities
    ///@{

    /// Velocity gradient
    VELOCITY_GRADIENT,

    /// Velocity divergence
    VELOCITY_DIVERGENCE,

    /// Velocity rotation
    VELOCITY_ROTATION,

    /// Correction tensor used to improve conservation of total angular momentum
    STRAIN_RATE_CORRECTION_TENSOR,

    /// Laplacian of internal energy, used in heat diffusion equation
    ENERGY_LAPLACIAN,

    ///@}

    /// \name Artificial velocity
    ///@{

    /// Coefficient alpha of the artificial viscosity. Coefficient beta is always 2*alpha.
    AV_ALPHA,

    /// Coefficient beta of the artificial viscosity
    AV_BETA,

    /// Balsara factor
    AV_BALSARA,

    /// Artificial stress by Monaghan \cite Monaghan_1999
    AV_STRESS,

    /// Auxiliary quantity needed for evaluating artificial stress
    INTERPARTICLE_SPACING_KERNEL,

    ///@}

    /// \name SPH modifications & corrections
    ///@{

    /// Grad-h terms, appear in self-consistent derivation of SPH equatios due to non-uniform smoothing
    /// lenghts.
    GRAD_H,

    /// XSPH corrections to velocity. Only modifies evolution equation for particle position, velocity (as an
    /// input for velocity divergence, ...) is NOT affected.
    XSPH_VELOCITIES,

    ///@}

    /// \name SPH statistics & auxiliary data
    ///@{

    /// Number of neighbouring particles (in radius h * kernel.radius)
    NEIGHBOUR_CNT,

    /// Vector approximating surface normal
    SURFACE_NORMAL,

    /// Initial position of particles, kept constant during the run
    INITIAL_POSITION,

    /// Smoothing lengths of particles. Note that ordinarily the smoothing lenghts are stored as 4th component
    /// of position vectors, so this ID cannot be used to obtain smoothing lenghts from Storage object. It can
    /// be useful for other uses of quantities, like data output, visualization etc.
    SMOOTHING_LENGHT,

    ///@}

    /// \name Particle flags & Materials
    ///@{

    /// ID of original body, used to implement discontinuities between bodies in SPH
    FLAG,

    /// Index of material of the particle. Can be generally different than the flag value.
    MATERIAL_ID,

    /// Persistent index of the particle that does not change when adding or removing particles in the
    /// storage. Useful when we need to track particle with given index; particle index in storage may change
    /// when some particles from the middle of the storage are removed. Indices of removed particles are made
    /// available again and can be reused by newly created particles.
    PERSISTENT_INDEX,

    ///@}


    // TEMPORARY QUANTITIES, TO BE REMOVED

    VELOCITY_LAPLACIAN,

    VELOCITY_GRADIENT_OF_DIVERGENCE,

    FRICTION,
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
    std::wstring label;

    /// \todo Units & dimensional analysis !

    /// \brief Variable expectedType contains a type the quantity will most likely have.
    ///
    /// The code does not assign fixed types to quantities, i.e. it's possible to create a tensor quantity
    /// QuantityId::DENSITY. This allows to use different modifications of SPH (tensor artificial viscosity,
    /// etc.), even though most quantities have only one type in any meaningful SPH simulation (density will
    /// always be scalar, for example).
    ValueEnum expectedType;

    QuantityMetadata(const std::string& fullName,
        const std::wstring& label,
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
