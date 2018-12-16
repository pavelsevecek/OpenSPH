#include "sph/initial/Presets.h"
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

enum class CollisionSettingsId {
    TARGET_RADIUS,
    TARGET_PARTICLE_CNT,
    MIN_PARTICLE_CNT,
    CENTER_OF_MASS_FRAME,
    IMPACTOR_RADIUS,
    IMPACT_SPEED,
    IMPACT_ANGLE,
    TARGET_ANGULAR_FREQUENCY,
    IMPACTOR_OFFSET,
    OPTIMIZE_IMPACTOR
};

using CollisionSettings = Settings<CollisionSettingsId>;

// clang-format off
template <>
AutoPtr<CollisionSettings> CollisionSettings::instance(new CollisionSettings{
    { CollisionSettingsId::TARGET_RADIUS,           "target_radius",        1.e4_f,
      "Radius of the target in meters." },
    { CollisionSettingsId::TARGET_PARTICLE_CNT,     "target_particle_cnt",  100000,
      "Approximate number of particles of the target. Number of impactor particles is inferred from "
      "the ratio of the target and impactor size." },
    { CollisionSettingsId::MIN_PARTICLE_CNT,        "min_particle_cnt",     100,
       "Minimal number of particles of the impactor, used to avoid creating unresolved impactor."},
    { CollisionSettingsId::CENTER_OF_MASS_FRAME,    "center_of_mass_frame", true,
       "If true, colliding bodies are moved to the center-of-mass system, otherwise the target is located "
        "at origin and has zero velocity." },
    { CollisionSettingsId::IMPACTOR_RADIUS,         "impactor_radius",      1.e3_f,
        "Radius of the impactor in meters" },
    { CollisionSettingsId::IMPACT_SPEED,            "impact_speed",         5.e3_f,
        "Relative impact speed (or absolute speed of the impactor if center-of-mass system is set to false) "
        "in meters per second." },
    { CollisionSettingsId::IMPACT_ANGLE,            "impact_angle",         45._f,
        "Impact angle, i.e. angle between normal at the point of impact and the velocity vector of the impactor. "
        "It can be negative to simulate retrograde impact. The angle is in degrees. "},
    { CollisionSettingsId::TARGET_ANGULAR_FREQUENCY, "target_angular_frequency", 0._f,
        "Initial angular frequency of the target around its z-axis in units rev/day." },
    { CollisionSettingsId::IMPACTOR_OFFSET,         "impactor_offset",      4,
        "Initial distance of the impactor from the target in units of smoothing length. The impactor should "
        "not be in contact with the target at the start of the simulation, so the value should be always larger "
        "than the radius of the selected kernel." },
    { CollisionSettingsId::OPTIMIZE_IMPACTOR,       "optimize_impactor",     true,
        "If true, some quantities of the impactor particles are not taken into account when computing the required "
        "time step. Otherwise, the time step might be unnecessarily too low, as the quantities in the impactor change "
        "rapidly. Note that this does not affect CFL criterion. "},
});
// clang-format on

template class Settings<CollisionSettingsId>;

bool Presets::CollisionParams::loadFromFile(const Path& path) {
    CollisionSettings settings;
    if (!settings.loadFromFile(path)) {
        return false;
    }
    targetRadius = settings.get<Float>(CollisionSettingsId::TARGET_RADIUS);
    targetRotation =
        settings.get<Float>(CollisionSettingsId::TARGET_ANGULAR_FREQUENCY) * 2._f * PI / (3600._f * 24._f);
    targetParticleCnt = settings.get<int>(CollisionSettingsId::TARGET_PARTICLE_CNT);
    minParticleCnt = settings.get<int>(CollisionSettingsId::MIN_PARTICLE_CNT);
    centerOfMassFrame = settings.get<bool>(CollisionSettingsId::CENTER_OF_MASS_FRAME);
    impactorRadius = settings.get<Float>(CollisionSettingsId::IMPACTOR_RADIUS);
    impactSpeed = settings.get<Float>(CollisionSettingsId::IMPACT_SPEED);
    impactAngle = settings.get<Float>(CollisionSettingsId::IMPACT_ANGLE) * DEG_TO_RAD;
    impactorOffset = settings.get<int>(CollisionSettingsId::IMPACTOR_OFFSET);
    optimizeImpactor = settings.get<bool>(CollisionSettingsId::OPTIMIZE_IMPACTOR);
    return true;
}

bool Presets::CollisionParams::saveToFile(const Path& path) {
    CollisionSettings settings;
    settings.set(CollisionSettingsId::TARGET_RADIUS, targetRadius);
    settings.set(
        CollisionSettingsId::TARGET_ANGULAR_FREQUENCY, targetRotation * 3600._f * 24._f / (2._f * PI));
    settings.set(CollisionSettingsId::TARGET_PARTICLE_CNT, int(targetParticleCnt));
    settings.set(CollisionSettingsId::MIN_PARTICLE_CNT, int(minParticleCnt));
    settings.set(CollisionSettingsId::CENTER_OF_MASS_FRAME, centerOfMassFrame);
    settings.set(CollisionSettingsId::IMPACTOR_RADIUS, impactorRadius);
    settings.set(CollisionSettingsId::IMPACT_SPEED, impactSpeed);
    settings.set(CollisionSettingsId::IMPACT_ANGLE, impactAngle * RAD_TO_DEG);
    settings.set(CollisionSettingsId::IMPACTOR_OFFSET, int(impactorOffset));
    settings.set(CollisionSettingsId::OPTIMIZE_IMPACTOR, optimizeImpactor);
    settings.saveToFile(path);
    return true;
}

Presets::Collision::Collision(IScheduler& scheduler,
    const RunSettings& settings,
    const CollisionParams& params)
    : _ic(scheduler, settings)
    , _params(params) {
    ASSERT(params.impactAngle >= -PI && params.impactAngle <= PI);
    ASSERT(params.impactSpeed >= 0._f);
    _params.body.set(BodySettingsId::PARTICLE_COUNT, int(_params.targetParticleCnt));
    // this has to match the actual center/velocity/rotation of the target below
    _params.body.set(BodySettingsId::BODY_CENTER, Vector(0._f));
    _params.body.set(BodySettingsId::BODY_VELOCITY, Vector(0._f));
    _params.body.set(BodySettingsId::BODY_ANGULAR_VELOCITY, Vector(0._f, 0._f, _params.targetRotation));
}

void Presets::Collision::addTarget(Storage& storage) {
    // make sure the value in settings is the came that's passed to params
    ASSERT(int(_params.targetParticleCnt) == _params.body.get<int>(BodySettingsId::PARTICLE_COUNT));
    ASSERT(_params.targetRadius > 0._f, "Target radius has not been initialized");
    SphericalDomain domain(Vector(0._f), _params.targetRadius);

    const Path targetPath = _params.outputPath / Path("target.sph");
    if (FileSystem::pathExists(targetPath)) {
        _params.body.loadFromFile(targetPath);
    } else {
        _params.body.saveToFile(targetPath);
    }

    if (_params.pebbleSfd) {
        ASSERT(!_params.concentration,
            "Arbitrary concentration is currently incompatible with rubble-pile target");
        ASSERT(_params.targetRotation == 0._f, "Rotation is currently incompatible with rubble-pile target");

        _ic.addRubblePileBody(storage, domain, _params.pebbleSfd.value(), _params.body);
    } else {

        BodyView view = [&] {
            if (_params.concentration) {
                // concentration specified, we have to use Diehl's distribution (no other distribution can
                // specify concentration)
                DiehlParams diehl;
                diehl.particleDensity = _params.concentration;
                diehl.maxDifference = _params.body.get<int>(BodySettingsId::DIEHL_MAX_DIFFERENCE);
                diehl.strength = _params.body.get<Float>(BodySettingsId::DIELH_STRENGTH);

                auto distr = makeAuto<DiehlDistribution>(diehl);
                return _ic.addMonolithicBody(
                    storage, domain, Factory::getMaterial(_params.body), std::move(distr));
            } else {
                // we can use the default distribution
                return _ic.addMonolithicBody(storage, domain, _params.body);
            }
        }();
        /// \todo the center of rotation (here Vector(0._f)) must match the center of domain above.
        view.addRotation(Vector(0._f, 0._f, _params.targetRotation), Vector(0._f));
    }
}

BodyView Presets::Collision::addImpactor(Storage& storage) {
    ASSERT(_params.impactorRadius > 0._f, "Impactor radius has not been initialized");
    const Float targetDensity = _params.targetParticleCnt / pow<3>(_params.targetRadius);
    const Float h = 1.f / root<3>(targetDensity);
    ASSERT(h > 0._f);

    Size impactorParticleCnt;
    if (_params.impactorParticleCntOverride) {
        impactorParticleCnt = _params.impactorParticleCntOverride.value();
    } else {
        impactorParticleCnt =
            max<int>(_params.minParticleCnt, targetDensity * pow<3>(_params.impactorRadius));
    }

    Vector center = this->getImpactPoint();
    // move impactor by some offset so that there is no overlap
    center[X] += _params.impactorOffset * h;
    const Vector v_imp(-_params.impactSpeed, 0._f, 0._f);

    BodySettings impactorBody = _params.body;
    impactorBody.set(BodySettingsId::PARTICLE_COUNT, int(impactorParticleCnt))
        .set(BodySettingsId::BODY_CENTER, center)
        .set(BodySettingsId::BODY_VELOCITY, v_imp)
        .set(BodySettingsId::BODY_ANGULAR_VELOCITY, Vector(0._f));
    if (_params.optimizeImpactor) {
        // check that we don't use it for similar bodies
        ASSERT(_params.impactorRadius < 0.5_f * _params.targetRadius);
        impactorBody.set(BodySettingsId::STRESS_TENSOR_MIN, LARGE).set(BodySettingsId::DAMAGE_MIN, LARGE);
    }

    const Path impactorPath = _params.outputPath / Path("impactor.sph");
    if (FileSystem::pathExists(impactorPath)) {
        impactorBody.loadFromFile(impactorPath);
    } else {
        impactorBody.saveToFile(impactorPath);
    }

    SphericalDomain domain(center, _params.impactorRadius);
    BodyView impactor = _ic.addMonolithicBody(storage, domain, impactorBody).addVelocity(v_imp);

    if (_params.centerOfMassFrame) {
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        Float m_total = 0._f;
        Vector r_com(0._f);
        Vector v_com(0._f);
        for (Size i = 0; i < m.size(); ++i) {
            m_total += m[i];
            r_com += m[i] * r[i];
            v_com += m[i] * v[i];
        }
        ASSERT(m_total != 0._f);
        v_com /= m_total;
        r_com /= m_total;
        // don't modify smoothing lengths
        r_com[H] = v_com[H] = 0._f;
        for (Size i = 0; i < m.size(); ++i) {
            r[i] -= r_com;
            v[i] -= v_com;
        }

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

Vector Presets::Collision::getImpactPoint() const {
    const Float impactorDistance = _params.targetRadius + _params.impactorRadius;
    const Float x = impactorDistance * cos(_params.impactAngle);
    const Float y = impactorDistance * sin(_params.impactAngle);
    return Vector(x, y, 0._f);
}

Presets::Satellite::Satellite(IScheduler& scheduler,
    ISolver& solver,
    const RunSettings& settings,
    const BodySettings& body,
    const SatelliteParams& params)
    : _ic(scheduler, solver, settings)
    , _body(body)
    , _params(params) {
    _body.set(BodySettingsId::PARTICLE_COUNT, int(_params.targetParticleCnt));
    // this has to match the actual center/velocity/rotation of the target below
    _body.set(BodySettingsId::BODY_CENTER, Vector(0._f));
    _body.set(BodySettingsId::BODY_VELOCITY, Vector(0._f));
    _body.set(BodySettingsId::BODY_ANGULAR_VELOCITY, _params.primaryRotation);
}

void Presets::Satellite::addPrimary(Storage& storage) {
    // make sure the value in settings is the came that's passed to params
    ASSERT(int(_params.targetParticleCnt) == _body.get<int>(BodySettingsId::PARTICLE_COUNT));
    SphericalDomain domain(Vector(0._f), _params.targetRadius);
    BodyView view = _ic.addMonolithicBody(storage, domain, _body);
    view.addRotation(_params.primaryRotation, Vector(0._f));
}

static Float getTotalMass(const Storage& storage) {
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    Float m_tot = 0._f;
    for (Size i = 0; i < m.size(); ++i) {
        m_tot += m[i];
    }
    return m_tot;
}

void Presets::Satellite::addSecondary(Storage& storage) {
    ASSERT(_params.satelliteRadius > EPS);
    const Float targetDensity = _params.targetParticleCnt / pow<3>(_params.targetRadius);
    Size satelliteParticleCnt =
        max<int>(_params.minParticleCnt, targetDensity * pow<3>(_params.satelliteRadius));

    const Float v_c = sqrt(Constants::gravity * getTotalMass(storage) / getLength(_params.satellitePosition));
    Vector v = getNormalized(_params.velocityDirection) * _params.velocityMultiplier * v_c;

    BodySettings satelliteBody = _body;
    satelliteBody.set(BodySettingsId::PARTICLE_COUNT, int(satelliteParticleCnt))
        .set(BodySettingsId::BODY_CENTER, _params.satellitePosition)
        .set(BodySettingsId::BODY_VELOCITY, v)
        .set(BodySettingsId::BODY_ANGULAR_VELOCITY, _params.satelliteRotation);
    SphericalDomain domain(_params.satellitePosition, _params.satelliteRadius);
    BodyView view = _ic.addMonolithicBody(storage, domain, satelliteBody);

    view.addVelocity(v);
    view.addRotation(_params.satelliteRotation, BodyView::RotationOrigin::CENTER_OF_MASS);
}

void Presets::addCloud(Storage& storage,
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
