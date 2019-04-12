#pragma once

/// \file Present.h
/// \brief Problem-specific initial conditions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "io/Path.h"
#include "objects/wrappers/Function.h"
#include "sph/initial/Initial.h"

NAMESPACE_SPH_BEGIN

enum class CollisionGeometrySettingsId {
    /// Radius of the parent body in meters
    TARGET_RADIUS,

    /// Approximate number of target particles. Actual number of generated particles might differ, depending
    /// on selected distribution.
    TARGET_PARTICLE_COUNT,

    /// Angular frequency of the target around z-axis in units rev/day.
    TARGET_SPIN_RATE,

    /// Radius of the projectile in meters
    IMPACTOR_RADIUS,

    /// Number of impactor particles. If zero, the number of particles is automatically computed based on the
    /// number of target particles and the ratio of target radius to projectile radius.
    IMPACTOR_PARTICLE_COUNT_OVERRIDE,

    /// Initial distance of the impactor from the impact point. This value is in units of smoothing length h.
    /// Should not be lower than kernel.radius() * eta.
    IMPACTOR_OFFSET,

    /// If true, derivatives in impactor will be computed with lower precision. This significantly improves
    /// the performance of the code. The option is intended mainly for cratering impacts and should be always
    /// false when simulating collision of bodies of comparable sizes.
    OPTIMIZE_IMPACTOR,

    /// Impact speed in m/s
    IMPACT_SPEED,

    /// Impact angle in degrees, i.e. angle between velocity vector and normal at the impact point.
    IMPACT_ANGLE,

    /// Minimal number of particles per body.
    MIN_PARTICLE_COUNT,

    /// If true, positions and velocities of particles are modified so that center of mass is at origin and
    /// has zero velocity.
    CENTER_OF_MASS_FRAME,
};

using CollisionGeometrySettings = Settings<CollisionGeometrySettingsId>;

/// \brief Holds all parameters specifying initial conditions of a collision simulation.
///
/// Note that all \ref Settings objects in this struct behave as overrides. All settings are associated with
/// configuration files; if the configuration file exists, the settings are loaded from it, otherwise the file
/// is created using default settings. These settings, either loaded from file or the defauls, can be then
/// overriden by settings in \ref CollisionParams. By default, the settings stored here are empty, meaning the
/// values loaded from the configuration files are used directly, without any modification. If the settings
/// are reset to defaults, this effectively disables loading of configuration files, as all loaded values
/// are overriden.
///
/// If the configuration file exists, but does not have a valid format or contains unknown values, an \ref
/// InvalidSetup exception is thrown.
struct CollisionParams {

    /// \brief Material parameters used for the target.
    ///
    /// These parameters are associated with configuration file "target.sph". Note that the material parameter
    /// \ref BodySettingsId::PARTICLE_COUNT is unused, as it is specified by collision parameter \ref
    /// CollisionId::TARGET_PARTICLE_COUNT instead.
    BodySettings targetBody = EMPTY_SETTINGS;

    /// \brief Material parameters used for the impactor.
    ///
    /// These parameters are associated with configuration file "impactor.sph". Note that the material
    /// parameter \ref BodySettingsId::PARTICLE_COUNT is unused, as it is specified by collision parameter
    /// \ref CollisionId::TARGET_PARTICLE_COUNT or \ref CollisionId::IMPACTOR_PARTICLE_COUNT instead.
    BodySettings impactorBody = EMPTY_SETTINGS;

    /// \brief Parameters describing the initial geometry of the two colliding bodies.
    ///
    /// These parameters are associated with configuration file "geometry.sph".
    CollisionGeometrySettings geometry = EMPTY_SETTINGS;

    /// \brief Path to the output directory.
    ///
    /// If set, configuration files are stored there.
    Path outputPath;

    /// \brief Logger used to notify about created bodies.
    ///
    /// May be nullptr.
    SharedPtr<ILogger> logger;

    /// \brief Function specifying particle concentration inside the target.
    ///
    /// If not specified, particles are spaced homogeneously.
    Function<Float(const Vector& r)> concentration;

    /// \brief Size distribution of the pebbles forming the rubble-pile target body.
    ///
    /// If NOTHING, the target is assumed to be monolithic.
    Optional<PowerLawSfd> pebbleSfd = NOTHING;
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
/// CollisionParams params;
/// params.collision.set(CollisionGeometrySettingsId::TARGET_RADIUS, 1.e5_f)    // R_pb = 100km
///                 .set(CollisionGeometrySettingsId::IMPACTOR_RADIUS, 2.e4_f)  // r_imp = 20km
///                 .set(CollisionGeometrySettingsId::IMPACT_SPEED, 5.e3_f)     // v_imp = 5km/s
///                 .set(CollisionGeometrySettingsId::IMPACT_ANGLE, 45._f);     // \phi_imp = 45°
///
/// CollisionInitialConditions setup(*ThreadPool::getGlobalInstance(), settings, params);
/// setup.addTarget(*storage);
/// setup.addImpactor(*storage);
/// \endcode
class CollisionInitialConditions : public Noncopyable {
private:
    InitialConditions ic;
    CollisionParams setup;

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
    /// \throw InvalidSetup if an error is encountered, for example if the configuration files are invalid.
    CollisionInitialConditions(IScheduler& scheduler,
        const RunSettings& settings,
        const CollisionParams& params);

    /// \brief Returns the final geometry settings used in the simulation.
    ///
    /// This is a combination of defaults or values loaded from configuration file and overrides passed in
    /// \ref CollisionParams in constructor.
    const CollisionGeometrySettings& getGeometry() const;

    /// \brief Returns the final material parameters of the target body.
    ///
    /// This is a combination of defaults or values loaded from configuration file and overrides passed in
    /// \ref CollisionParams in constructor.
    const BodySettings& getTargetBody() const;

    /// \brief Returns the final material parameters of the impactor body.
    ///
    /// This is a combination of defaults or values loaded from configuration file and overrides passed in
    /// \ref CollisionParams in constructor.
    const BodySettings& getImpactorBody() const;

    /// \brief Returns the position of the impact point.
    Vector getImpactPoint() const;

    /// \brief Adds a target (primary body) into the storage.
    ///
    /// Can be called only once for given storage. Unless the center-of-mass system is specified in \ref
    /// CollisionParams, the target is created at the origin and has zero velocity.
    /// \param storage Particle storage where the target body is added.
    void addTarget(Storage& storage);

    /// \brief Manually adds a target (primary body) into the storage.
    ///
    /// The function effectively merges the two storages (by moving the target into the storage). Useful to
    /// create a different target than the one generated by \ref addTarget (for example by loading a state
    /// file, setting up more complex rubble-pile target, etc.), but still use \ref CollisionInitialCondition
    /// to create and properly set up an impactor.
    ///
    /// \note User must call either \ref addTarget or \ref addCustom target (but not both) before calling \ref
    /// addImpactor.
    void addCustomTarget(Storage& storage, Storage&& target);

    /// \brief Adds an impactor (secondary body) into the storage.
    ///
    /// Can be called only once for given storage. Must be called after \ref addTarget or \ref addCustomTarget
    /// have been called.
    /// \param storage Particle storage where the impactor body is added.
    /// \return View of the impactor particles, which can be used to spin it up, change its velocity, etc.
    BodyView addImpactor(Storage& storage);

private:
    void setTargetParams(BodySettings overrides);
    void setImpactorParams(BodySettings overrides);
};

struct CloudParams {
    Float cloudRadius;

    Float totalMass;

    Float particleRadius;

    Size particleCnt;

    Float radialExponent = 0.5_f;
};

void setupCloudInitialConditions(Storage& storage,
    ISolver& solver,
    const RunSettings& settings,
    const BodySettings& body,
    const CloudParams& params);


NAMESPACE_SPH_END
