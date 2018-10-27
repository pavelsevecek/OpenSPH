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
    /// \brief Radius of the parent body in meters
    Float targetRadius = 0._f;

    /// \brief Number of target particles.
    Size targetParticleCnt = 100000;

    /// \brief Minimal number of particles per body
    Size minParticleCnt = 100;

    /// \brief If true, positions and velocities of particles are modified so that center of mass is at origin
    /// and has zero velocity.
    bool centerOfMassFrame = false;

    /// \brief Path to the output directory.
    ///
    /// If set, parameters of target and impactor are saved there.
    Path outputPath;

    /// \brief Function specifying particle concentration inside the target.
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

    /// \brief Material parameters used for both the target and the impactor.
    ///
    /// Note that the parameter \ref BodySettingsId::PARTICLE_COUNT is overriden by targetParticleCnt for
    /// target, and similarly by impactorParticleCntOverride for impactor.
    BodySettings body;

    /// \brief Radius of the projectile in meters
    Float impactorRadius = 0._f;

    /// \brief Impact speed in m/s
    Float impactSpeed = 0._f;

    /// \brief Impact angle, i.e. angle between velocity vector and normal at the impact point
    Float impactAngle = 0._f;

    /// \brief Angular frequency of the target around z-axis.
    Float targetRotation = 0._f;

    /// \brief Size distribution of the pebbles forming the rubble-pile target body.
    ///
    /// If NOTHING, the target is assumed to be monolithic.
    Optional<PowerLawSfd> pebbleSfd = NOTHING;

    /// \brief Number of impactor particles.
    ///
    /// If unused (value is NOTHING), the number of particles is automatically computed based on the
    /// number of target particles and the ratio of target radius to projectile radius.
    Optional<Size> impactorParticleCntOverride = NOTHING;

    /// \brief Initial distance of the impactor from the impact point.
    ///
    /// This value is in units of smoothing length h. Should not be lower than kernel.radius() * eta.
    Float impactorOffset = 6._f;

    /// \brief If true, derivatives in impactor will be computed with lower precision.
    ///
    /// This significantly improves the performance of the code. The option is intended mainly for cratering
    /// impacts and should be always false when simulating collision of bodies of comparable sizes.
    bool optimizeImpactor = true;

    bool loadFromFile(const Path& path);
};

/// \brief Class for setting up initial conditions of asteroid impact.
///
/// This provides the simplest way to set up initial condition for an impact. It is only necessary to fill up
/// the \ref CollisionParams structure, specifying the dimensions of the colliding bodies, impact speed,
/// material parameters, etc. To fill the particle storage with particles of the target and impactor, use the
/// following in your implementation of \ref IRun::setUp:
/// \code
/// storage = makeShared<Storage>(); // no need to specify a material
///
/// Presets::CollisionParams params;
/// params.targetRadius   = 1.e5_f;             // R_pb = 100km
/// params.impactorRadius = 2.e4_f;             // r_imp = 20km
/// params.impactSpeed    = 5.e3_f;             // v_imp = 5km/s
/// params.impactAngle    = 45._f * DEG_TO_RAD; // \phi_imp = 45°
///
/// Presets::Collision setup(*ThreadPool::getGlobalInstance(), settings, params);
/// setup.addTarget(*setup);
/// setup.addImpactor(*setup);
/// \endcode
class Collision : public Noncopyable {
private:
    InitialConditions _ic;
    CollisionParams _params;

public:
    /// \brief Creates a new collision setup.
    ///
    /// While the object has no connection to a specific \ref Storage, it keeps an internal counter of the
    /// number of created bodies, so it should be only used with one storage. Create a new instance of this
    /// class to add another target or impactor to a different storage.
    /// \param scheduler Scheduler used for parallelization; use \ref ThreadPool::getGlobalInstance unless you
    ///                  know what you are doing.
    /// \param settings Settings of the simulation. Used for determined the used SPH kernel, for example.
    /// \param params Parameters of the collision; see \ref CollisionParams struct.
    Collision(IScheduler& scheduler, const RunSettings& settings, const CollisionParams& params);

    /// \brief Adds a target (primary body) into the storage.
    ///
    /// Can be called only once for given storage. Unless the center-of-mass system is specified in \ref
    /// CollisionParams, the target is created at the origin and has zero velocity.
    /// \param storage Particle storage where the target body is added.
    void addTarget(Storage& storage);

    /// \brief Adds an impactor (secondary body) into the storage.
    ///
    /// Can be called only once for given storage. Must be called after \ref addTarget has been called.
    /// \param storage Particle storage where the impactor body is added.
    /// \return View of the impactor particles, which can be used to spin it up, change its velocity, etc.
    BodyView addImpactor(Storage& storage);

    /// \brief Returns the position of the impact point.
    Vector getImpactPoint() const;
};


struct SatelliteParams : public TwoBodyParams {
    /// \brief Initial position of the satellite.
    Vector satellitePosition;

    /// \brief Radius of the satellite.
    Float satelliteRadius;

    /// \brief Initial velocity of the satellite.
    Vector velocityDirection;

    /// \brief Multiplicative factor of the Keplerian velocity.
    Float velocityMultiplier = 1._f;

    Vector primaryRotation = Vector(0._f);

    /// \brief Initial angular frequency of the satellite.
    Vector satelliteRotation = Vector(0._f);
};

class Satellite : public Noncopyable {
private:
    InitialConditions _ic;
    BodySettings _body;
    SatelliteParams _params;

public:
    Satellite(IScheduler& scheduler,
        ISolver& solver,
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


struct CloudParams {
    Float cloudRadius;

    Float totalMass;

    Float particleRadius;

    Size particleCnt;

    Float radialExponent = 0.5_f;
};

void addCloud(Storage& storage,
    ISolver& solver,
    const RunSettings& settings,
    const BodySettings& body,
    const CloudParams& params);

} // namespace Presets

NAMESPACE_SPH_END
