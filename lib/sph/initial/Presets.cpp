#include "sph/initial/Presets.h"
#include "io/Logger.h"
#include "objects/geometry/Domain.h"
#include "physics/Constants.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"
#include "system/Settings.impl.h"
#include "thread/Scheduler.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

// clang-format off
template <>
AutoPtr<CollisionGeometrySettings> CollisionGeometrySettings::instance(new CollisionGeometrySettings{
    { CollisionGeometrySettingsId::TARGET_RADIUS,          "target_radius",            1.e4_f,
        "Radius of the target in meters." },
    { CollisionGeometrySettingsId::TARGET_PARTICLE_COUNT,  "target_particle_cnt",    100000,
        "Approximate number of particles of the target. Number of impactor particles is inferred from "
        "the ratio of the target and impactor size." },
    { CollisionGeometrySettingsId::TARGET_SPIN_RATE,       "target_angular_frequency",         0._f,
        "Initial angular frequency of the target around its z-axis in units rev/day." },
    { CollisionGeometrySettingsId::MIN_PARTICLE_COUNT,     "min_particle_cnt",         100,
        "Minimal number of particles of the impactor, used to avoid creating unresolved impactor."},
    { CollisionGeometrySettingsId::CENTER_OF_MASS_FRAME,   "center_of_mass_frame",     false,
        "If true, colliding bodies are moved to the center-of-mass system, otherwise the target is located "
        "at origin and has zero velocity." },
    { CollisionGeometrySettingsId::IMPACTOR_RADIUS,        "impactor_radius",          1.e3_f,
        "Radius of the impactor in meters." },
    { CollisionGeometrySettingsId::IMPACTOR_PARTICLE_COUNT_OVERRIDE, "impactor_particle_count_override", 0,
        "Number of impactor particles. If zero, the number of particles is automatically computed based on the "
        "number of target particles and the ratio of target radius to projectile radius."},
    { CollisionGeometrySettingsId::OPTIMIZE_IMPACTOR,      "optimize_impactor",        true,
        "If true, some quantities of the impactor particles are not taken into account when computing the required "
        "time step. Otherwise, the time step might be unnecessarily too low, as the quantities in the impactor change "
        "rapidly. Note that this does not affect CFL criterion. "},
    { CollisionGeometrySettingsId::IMPACTOR_OFFSET,        "impactor_offset",          4._f,
        "Initial distance of the impactor from the target in units of smoothing length. The impactor should "
        "not be in contact with the target at the start of the simulation, so the value should be always larger "
        "than the radius of the selected kernel." },
    { CollisionGeometrySettingsId::IMPACT_SPEED,           "impact_speed",             5.e3_f,
        "Relative impact speed (or absolute speed of the impactor if center-of-mass system is set to false) "
        "in meters per second." },
    { CollisionGeometrySettingsId::IMPACT_ANGLE,           "impact_angle",             45._f,
        "Impact angle, i.e. angle between normal at the point of impact and the velocity vector of the impactor. "
        "It can be negative to simulate retrograde impact. The angle is in degrees. "},
});
// clang-format on

template class Settings<CollisionGeometrySettingsId>;

/// \brief Returns the numerical particle density (concentration) of the target
static Float getTargetDensity(const CollisionGeometrySettings& collision) {
    const Float targetParticleCnt = collision.get<int>(CollisionGeometrySettingsId::TARGET_PARTICLE_COUNT);
    const Float targetRadius = collision.get<Float>(CollisionGeometrySettingsId::TARGET_RADIUS);
    return targetParticleCnt / pow<3>(targetRadius);
}

/// \brief Sets up material parameters of a body.
///
/// \param out Final material parameters
/// \param bodySpecific Parameters specific for the body. These override the defaults, but they are overriden
///                     by values loaded from the configuration file.
/// \param overrides Overrides applied on top of body-specific parameters. They also override values loaded
///                  from the configuration file.
/// \param path Path to the configuration file.
static Expected<bool> setBodyParams(BodySettings& out,
    const BodySettings& bodySpecific,
    const BodySettings& overrides,
    const Path& path) {

    // sanity check to ensure we don't override something we don't want to; can be modified if more values are
    // added in the future
    ASSERT(bodySpecific.size() < 5);

    // set to defaults (to fill all entries with something)
    out = BodySettings::getDefaults();

    // override with collision-specific values, shared for both bodies
    out.set(BodySettingsId::ENERGY, 1.e3_f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(0._f, INFTY))
        .set(BodySettingsId::EOS, EosEnum::TILLOTSON)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES)
        .set(BodySettingsId::DISTRIBUTE_MODE_SPH5, false)
        .set(BodySettingsId::STRESS_TENSOR_MIN, 4.e6_f)
        .set(BodySettingsId::ENERGY_MIN, 10._f)
        .set(BodySettingsId::DAMAGE_MIN, 0.25_f);

    // add body specific values - these can be overriden by the value loaded from configuration file
    out.addEntries(bodySpecific);

    // remove particle count - this value is always overriden by value specified in geometry settings
    out.unset(BodySettingsId::PARTICLE_COUNT);

    // either save the defaults to configuration file, or override the defaults with values loaded from
    // configuration file
    return out.tryLoadFileOrSaveCurrent(path, overrides);
}

static void checkResult(const Expected<bool> loaded, const Path& path, ILogger& logger) {
    const std::string desc = path.fileName().removeExtension().native();
    if (!loaded) {
        throw InvalidSetup(
            "Cannot load " + desc + " configuration file " + path.native() + "\n\n" + loaded.error());
    }
    if (loaded.value()) {
        logger.write("Loaded " + desc + " settings from file '", path.native(), "'");
    } else {
        logger.write("No " + desc + " settings found, defaults saved to file '", path.native(), "'");
    }
}

void CollisionInitialConditions::setTargetParams(BodySettings overrides) {
    // this has to match the actual center/velocity/rotation of the target, as specified by the collision
    // parameters.
    /// \todo try to get rid of these; center and velocity are trivial, angular frequency can be determined
    /// from angular momentum.
    overrides.set(BodySettingsId::BODY_CENTER, Vector(0._f));
    overrides.set(BodySettingsId::BODY_VELOCITY, Vector(0._f));

    const Float targetSpinRate = setup.geometry.get<Float>(CollisionGeometrySettingsId::TARGET_SPIN_RATE) *
                                 2._f * PI / (3600._f * 24._f);
    overrides.set(BodySettingsId::BODY_SPIN_RATE, Vector(0._f, 0._f, targetSpinRate));

    const Path targetPath = setup.outputPath / Path("target.sph");
    const Expected<bool> loaded = setBodyParams(setup.targetBody, EMPTY_SETTINGS, overrides, targetPath);
    checkResult(loaded, targetPath, *setup.logger);
}

void CollisionInitialConditions::setImpactorParams(BodySettings overrides) {
    const Float targetDensity = getTargetDensity(setup.geometry);
    const Float h = 1.f / root<3>(targetDensity);
    ASSERT(h > 0._f);
    Vector center = this->getImpactPoint();
    // move impactor by some offset so that there is no overlap
    center[X] += setup.geometry.get<Float>(CollisionGeometrySettingsId::IMPACTOR_OFFSET) * h;
    overrides.set(BodySettingsId::BODY_CENTER, center);

    const Float impactSpeed = setup.geometry.get<Float>(CollisionGeometrySettingsId::IMPACT_SPEED);
    const Vector v_imp(-impactSpeed, 0._f, 0._f);
    overrides.set(BodySettingsId::BODY_VELOCITY, v_imp);

    overrides.set(BodySettingsId::BODY_SPIN_RATE, Vector(0._f));

    BodySettings impactorBody = EMPTY_SETTINGS;
    if (setup.geometry.get<bool>(CollisionGeometrySettingsId::OPTIMIZE_IMPACTOR)) {
        // check that we don't use it for similar bodies
        const Float targetRadius = setup.geometry.get<Float>(CollisionGeometrySettingsId::TARGET_RADIUS);
        const Float impactorRadius = setup.geometry.get<Float>(CollisionGeometrySettingsId::IMPACTOR_RADIUS);
        ASSERT(impactorRadius < 0.5_f * targetRadius);
        impactorBody.set(BodySettingsId::STRESS_TENSOR_MIN, LARGE).set(BodySettingsId::DAMAGE_MIN, LARGE);
    }

    const Path impactorPath = setup.outputPath / Path("impactor.sph");
    const Expected<bool> loaded = setBodyParams(setup.impactorBody, impactorBody, overrides, impactorPath);
    checkResult(loaded, impactorPath, *setup.logger);
}

CollisionInitialConditions::CollisionInitialConditions(IScheduler& scheduler,
    const RunSettings& settings,
    const CollisionParams& params)
    : ic(scheduler, settings)
    , setup(params) {
    // sanitize logger
    if (!setup.logger) {
        setup.logger = makeAuto<NullLogger>();
    }

    setup.geometry = CollisionGeometrySettings::getDefaults();
    const Path geometryPath = setup.outputPath / Path("geometry.sph");
    const Expected<bool> loaded = setup.geometry.tryLoadFileOrSaveCurrent(geometryPath, params.geometry);
    checkResult(loaded, geometryPath, *setup.logger);

    setTargetParams(params.targetBody);
    setImpactorParams(params.impactorBody);
}

const CollisionGeometrySettings& CollisionInitialConditions::getGeometry() const {
    return setup.geometry;
}

const BodySettings& CollisionInitialConditions::getTargetBody() const {
    return setup.targetBody;
}

const BodySettings& CollisionInitialConditions::getImpactorBody() const {
    return setup.impactorBody;
}

void CollisionInitialConditions::addTarget(Storage& storage) {
    ASSERT(storage.getParticleCnt() == 0);

    const Float targetRadius = setup.geometry.get<Float>(CollisionGeometrySettingsId::TARGET_RADIUS);
    SphericalDomain domain(Vector(0._f), targetRadius);

    const int particleCnt = setup.geometry.get<int>(CollisionGeometrySettingsId::TARGET_PARTICLE_COUNT);
    setup.targetBody.set(BodySettingsId::PARTICLE_COUNT, particleCnt);

    if (setup.pebbleSfd) {
        ASSERT(!setup.concentration,
            "Arbitrary concentration is currently incompatible with rubble-pile target");
        ASSERT(setup.geometry.get<Float>(CollisionGeometrySettingsId::TARGET_SPIN_RATE) == 0._f,
            "Rotation is currently incompatible with rubble-pile target");

        ic.addRubblePileBody(storage, domain, setup.pebbleSfd.value(), setup.targetBody);
    } else {

        BodyView view = [&] {
            if (setup.concentration) {
                // concentration specified, we have to use Diehl's distribution (no other distribution can
                // specify concentration)
                DiehlParams diehl;
                diehl.particleDensity = setup.concentration;
                diehl.maxDifference = setup.targetBody.get<int>(BodySettingsId::DIEHL_MAX_DIFFERENCE);
                diehl.strength = setup.targetBody.get<Float>(BodySettingsId::DIELH_STRENGTH);

                auto distr = makeAuto<DiehlDistribution>(diehl);
                return ic.addMonolithicBody(
                    storage, domain, Factory::getMaterial(setup.targetBody), std::move(distr));
            } else {
                // we can use the default distribution
                return ic.addMonolithicBody(storage, domain, setup.targetBody);
            }
        }();

        const Vector center = setup.targetBody.get<Vector>(BodySettingsId::BODY_CENTER);
        const Vector spinRate = setup.targetBody.get<Vector>(BodySettingsId::BODY_SPIN_RATE);
        view.addRotation(spinRate, center);
    }
}

void CollisionInitialConditions::addCustomTarget(Storage& storage, Storage&& target) {
    storage.merge(std::move(target));

    /// \todo possibly do a hack-free implementation
    Storage dummy;
    this->addTarget(dummy);
}

BodyView CollisionInitialConditions::addImpactor(Storage& storage) {
    ASSERT(storage.getParticleCnt() > 0 && storage.getMaterialCnt() == 1);

    // compute required particle count
    const Float impactorRadius = setup.geometry.get<Float>(CollisionGeometrySettingsId::IMPACTOR_RADIUS);
    Size impactorParticleCnt =
        setup.geometry.get<int>(CollisionGeometrySettingsId::IMPACTOR_PARTICLE_COUNT_OVERRIDE);
    if (impactorParticleCnt == 0) {
        const int minParticleCnt = setup.geometry.get<int>(CollisionGeometrySettingsId::MIN_PARTICLE_COUNT);
        const Float targetDensity = getTargetDensity(setup.geometry);
        impactorParticleCnt = max<int>(1, minParticleCnt, int(targetDensity * pow<3>(impactorRadius)));
    }

    /// \todo move to constructor, so that we have actual number of particles when getImpactorBody is called?
    setup.impactorBody.set(BodySettingsId::PARTICLE_COUNT, int(impactorParticleCnt));

    const Vector center = setup.impactorBody.get<Vector>(BodySettingsId::BODY_CENTER);
    SphericalDomain domain(center, impactorRadius);
    BodyView impactor = ic.addMonolithicBody(storage, domain, setup.impactorBody);

    const Vector v_imp = setup.impactorBody.get<Vector>(BodySettingsId::BODY_VELOCITY);
    impactor.addVelocity(v_imp);

    if (setup.geometry.get<bool>(CollisionGeometrySettingsId::CENTER_OF_MASS_FRAME)) {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        const Vector r_com = moveToCenterOfMassSystem(m, r);
        const Vector v_com = moveToCenterOfMassSystem(m, v);

        // modify body metadata - this will not be saved to configuration files, but it may be used by some
        // code components (corotating velocity colorizer, for example)
        for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
            MaterialView mat = storage.getMaterial(matId);
            mat->setParam(
                BodySettingsId::BODY_CENTER, mat->getParam<Vector>(BodySettingsId::BODY_CENTER) - r_com);
            mat->setParam(
                BodySettingsId::BODY_VELOCITY, mat->getParam<Vector>(BodySettingsId::BODY_VELOCITY) - v_com);
        }
    }

    return impactor;
}

Vector CollisionInitialConditions::getImpactPoint() const {
    const Float targetRadius = setup.geometry.get<Float>(CollisionGeometrySettingsId::TARGET_RADIUS);
    const Float impactorRadius = setup.geometry.get<Float>(CollisionGeometrySettingsId::IMPACTOR_RADIUS);
    const Float impactorDistance = targetRadius + impactorRadius;

    const Float impactAngle =
        setup.geometry.get<Float>(CollisionGeometrySettingsId::IMPACT_ANGLE) * DEG_TO_RAD;
    ASSERT(impactAngle >= -PI && impactAngle <= PI, impactAngle);
    const Float x = impactorDistance * cos(impactAngle);
    const Float y = impactorDistance * sin(impactAngle);
    return Vector(x, y, 0._f);
}


void setupCloudInitialConditions(Storage& storage,
    ISolver& solver,
    const RunSettings& settings,
    const BodySettings& body,
    const CloudParams& params) {

    AutoPtr<IRng> rng = Factory::getRng(settings);
    Array<Vector> r, v;

    for (Size i = 0; i < params.particleCnt; ++i) {
        const Float phi = 2._f * PI * (*rng)(0);
        const Float rad = params.cloudRadius * pow((*rng)(1), params.radialExponent);
        const Vector pos(rad * cos(phi), rad * sin(phi), 0._f, params.particleRadius);
        r.push(pos);

        /// \todo this is only true for uniform distribution!!
        const Float M = params.totalMass * sqr(rad) / sqr(params.cloudRadius);
        const Float v_kep = sqrt(Constants::gravity * M / rad);
        v.push(v_kep * cross(Vector(0._f, 0._f, 1._f), pos / rad));
    }
    repelParticles(r, 4._f);

    Storage cloud(Factory::getMaterial(body));
    cloud.insert<Vector>(QuantityId::POSITION, OrderEnum::SECOND, std::move(r));
    cloud.getDt<Vector>(QuantityId::POSITION) = std::move(v);

    cloud.insert<Float>(QuantityId::MASS, OrderEnum::ZERO, params.totalMass / params.particleCnt);
    solver.create(cloud, cloud.getMaterial(0));

    storage.merge(std::move(cloud));
}

NAMESPACE_SPH_END
