#pragma once

/// \file Present.h
/// \brief Problem-specific initial conditions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "io/Path.h"
#include "objects/wrappers/Function.h"
#include "sph/initial/Initial.h"

NAMESPACE_SPH_BEGIN

namespace Presets {

struct TwoBodyParams {
    /// Radius of the parent body in meters
    Float targetRadius;

    /// Number of target particles
    Size targetParticleCnt = 100000;

    /// Minimal number of particles per body
    Size minParticleCnt = 100;

    /// If true, positions and velocities of particles are modified so that center of mass is at origin and
    /// has zero velocity.
    bool centerOfMassFrame = false;

    /// Path to the output directory.
    ///
    /// If set, parameters of target and impactor are saved there.
    Path outputPath;

    /// Function specifying particle concentration inside the target.
    ///
    /// If not specified, particles are spaced homogeneously.
    Function<Float(const Vector& r)> concentration;
};


enum class CollisionSettingsId {
    TARGET_RADIUS,
    TARGET_PARTICLE_CNT,
    MIN_PARTICLE_CNT,
    CENTER_OF_MASS_FRAME,
    IMPACTOR_RADIUS,
    IMPACT_SPEED,
    IMPACT_ANGLE,
    TARGET_ROTATION,
    IMPACTOR_OFFSET,
    OPTIMIZE_IMPACTOR
};

using CollisionSettings = Settings<CollisionSettingsId>;

struct CollisionParams : public TwoBodyParams {

    /// Radius of the projectile in meters
    Float impactorRadius;

    /// Impact speed in m/s
    Float impactSpeed;

    /// Impact angle, i.e. angle between velocity vector and normal at the impact point
    Float impactAngle;

    /// Angular frequency of the target around z-axis.
    Float targetRotation = 0._f;

    /// Number of impactor particles.
    ///
    /// If unused (value is NOTHING), the number of particles is automatically computed based on the
    /// number of target particles and the ratio of target radius to projectile radius.
    Optional<Size> impactorParticleCntOverride = NOTHING;

    /// Initial distance of the impactor from the impact point.
    ///
    /// This value is in units of smoothing length h. Should not be lower than kernel.radius() * eta.
    Float impactorOffset = 6._f;

    /// If true, derivatives in impactor will be computed with lower precision.
    ///
    /// This significantly improves the performance of the code. The option is intended mainly for cratering
    /// impacts and should be always false when simulating collision of bodies of comparable sizes.
    bool optimizeImpactor = true;

    bool loadFromFile(const Path& path);
};

/// \brief Object for setting up initial conditions of asteroid impact.
class Collision : public Noncopyable {
private:
    InitialConditions _ic;
    BodySettings _body;
    CollisionParams _params;

public:
    Collision(const RunSettings& settings, const BodySettings& body, const CollisionParams& params);

    /// \brief Adds a target (primary body) into the storage.
    ///
    /// Can be called only once for given storage.
    void addTarget(Storage& storage);

    /// \brief Adds an impactor (secondary body) into the storage.
    ///
    /// Can be called only once for given storage. Must be called after \ref addTarget has been called.
    void addImpactor(Storage& storage);

    /// \bref Returns the position of the impact point.
    Vector getImpactPoint() const;
};

struct SatelliteParams : public TwoBodyParams {
    /// Initial position of the satellite.
    Vector satellitePosition;

    /// Radius of the satellite.
    Float satelliteRadius;

    /// Initial velocity of the satellite.
    Vector velocityDirection;

    /// Multiplicative factor of the Keplerian velocity.
    Float velocityMultiplier = 1._f;

    Vector primaryRotation = Vector(0._f);

    /// Initial angular frequency of the satellite.
    Vector satelliteRotation = Vector(0._f);
};

class Satellite : public Noncopyable {
private:
    InitialConditions _ic;
    BodySettings _body;
    SatelliteParams _params;

public:
    Satellite(ISolver& solver,
        const RunSettings& settings,
        const BodySettings& body,
        const SatelliteParams& params);

    /// \brief Adds the primary body into the storage.
    ///
    /// Can be called only once for given storage.
    void addPrimary(Storage& storage);

    /// \brief Adds the secondary body into the storage.
    ///
    /// Can be called only once for given storage. Must be called after \ref addPrimary has been called.
    void addSecondary(Storage& storage);
};

} // namespace Presets

NAMESPACE_SPH_END
