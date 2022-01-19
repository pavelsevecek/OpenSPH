#pragma once

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

enum class ParticleInteractionEnum {
    /// No interaction
    NONE,

    /// Particles are absorbed
    ABSORB,

    /// Particles are repelled
    REPEL,
};

enum class AttractorSettingsId {
    /// String identifier of the attractor
    LABEL = 0,

    // deprecated
    BLACK_HOLE = 1,

    /// Specifies how the attractor interacts with particle
    INTERACTION = 2,

    /// Visible when rendered
    VISIBLE = 99,

    /// Texture
    VISUALIZATION_TEXTURE = 100,
};

using AttractorSettings = Settings<AttractorSettingsId>;

/// \brief Single point-mass particle.
///
/// Extra properties of the attractor can be stored in the settings member variable.
struct Attractor {
    Vector position;
    Vector velocity = Vector(0._f);
    Vector acceleration = Vector(0._f);
    Float radius;
    Float mass;

    AttractorSettings settings = EMPTY_SETTINGS;

    Attractor() = default;

    Attractor(const Vector& position, const Vector& velocity, const Float radius, const Float mass)
        : position(position)
        , velocity(velocity)
        , radius(radius)
        , mass(mass) {}

    /// \brief Evaluates interactions of the attractor with all particles.
    void interact(IScheduler& scheduler, Storage& storage);
};

NAMESPACE_SPH_END
