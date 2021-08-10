#pragma once

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

enum class AttractorSettingsId {
    /// If true, the attractor absorbs any particle that falls below the attractor's radius.
    BLACK_HOLE = 0,

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
};

NAMESPACE_SPH_END
