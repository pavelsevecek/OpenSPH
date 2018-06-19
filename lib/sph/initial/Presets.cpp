#include "sph/initial/Presets.h"
#include "objects/geometry/Domain.h"
#include "physics/Constants.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"
#include "system/Settings.impl.h"

NAMESPACE_SPH_BEGIN

template <>
AutoPtr<Presets::CollisionSettings> Presets::CollisionSettings::instance(new Presets::CollisionSettings{
    /// Renderer
    { Presets::CollisionSettingsId::TARGET_RADIUS, "target_radius", 1.e4_f },
    { Presets::CollisionSettingsId::TARGET_PARTICLE_CNT, "target_particle_cnt", 100000 },
    { Presets::CollisionSettingsId::MIN_PARTICLE_CNT, "min_particle_cnt", 100 },
    { Presets::CollisionSettingsId::CENTER_OF_MASS_FRAME, "center_of_mass_frame", true },
    { Presets::CollisionSettingsId::IMPACTOR_RADIUS, "impactor_radius", 1.e3_f },
    { Presets::CollisionSettingsId::IMPACT_SPEED, "impact_speed", 5.e3_f },
    { Presets::CollisionSettingsId::IMPACT_ANGLE, "impact_angle", 45._f * DEG_TO_RAD },
    { Presets::CollisionSettingsId::TARGET_ROTATION, "target_rotation", 0._f },
    { Presets::CollisionSettingsId::IMPACTOR_OFFSET, "impactor_offset", 4 },
    { Presets::CollisionSettingsId::OPTIMIZE_IMPACTOR, "optimize_impactor", true },
});

template class Settings<Presets::CollisionSettingsId>;

bool Presets::CollisionParams::loadFromFile(const Path& path) {
    CollisionSettings settings;
    if (!settings.loadFromFile(path)) {
        return false;
    }
    targetRadius = settings.get<Float>(CollisionSettingsId::TARGET_RADIUS);
    targetParticleCnt = settings.get<int>(CollisionSettingsId::TARGET_PARTICLE_CNT);
    minParticleCnt = settings.get<int>(CollisionSettingsId::MIN_PARTICLE_CNT);
    centerOfMassFrame = settings.get<bool>(CollisionSettingsId::CENTER_OF_MASS_FRAME);
    impactorRadius = settings.get<Float>(CollisionSettingsId::IMPACTOR_RADIUS);
    impactSpeed = settings.get<Float>(CollisionSettingsId::IMPACT_SPEED);
    impactAngle = settings.get<Float>(CollisionSettingsId::IMPACT_ANGLE);
    impactorOffset = settings.get<int>(CollisionSettingsId::IMPACTOR_OFFSET);
    optimizeImpactor = settings.get<bool>(CollisionSettingsId::OPTIMIZE_IMPACTOR);
    return true;
}

Presets::Collision::Collision(const RunSettings& settings,
    const BodySettings& body,
    const CollisionParams& params)
    : _ic(settings)
    , _body(body)
    , _params(params) {
    ASSERT(params.impactAngle >= 0._f && params.impactAngle < 2._f * PI);
    ASSERT(params.impactSpeed >= 0._f);
    _body.set(BodySettingsId::PARTICLE_COUNT, int(_params.targetParticleCnt));
    // this has to match the actual center/velocity/rotation of the target below
    _body.set(BodySettingsId::BODY_CENTER, Vector(0._f));
    _body.set(BodySettingsId::BODY_VELOCITY, Vector(0._f));
    _body.set(BodySettingsId::BODY_ANGULAR_VELOCITY, Vector(0._f, 0._f, _params.targetRotation));
}

void Presets::Collision::addTarget(Storage& storage) {
    // make sure the value in settings is the came that's passed to params
    ASSERT(int(_params.targetParticleCnt) == _body.get<int>(BodySettingsId::PARTICLE_COUNT));
    SphericalDomain domain(Vector(0._f), _params.targetRadius);
    BodyView view = [&] {
        if (_params.concentration) {
            // concentration specified, we have to use Diehl's distribution (no other distribution can
            // specify concentration)
            const Float strength = _body.get<Float>(BodySettingsId::DIELH_STRENGTH);
            const Size diff = _body.get<int>(BodySettingsId::DIEHL_MAX_DIFFERENCE);
            AutoPtr<DiehlDistribution> distr =
                makeAuto<DiehlDistribution>(_params.concentration, diff, 50, strength);
            return _ic.addMonolithicBody(storage, domain, Factory::getMaterial(_body), std::move(distr));
        } else {
            // we can use the default distribution
            return _ic.addMonolithicBody(storage, domain, _body);
        }
    }();
    /// \todo the center of rotation (here Vector(0._f)) must match the center of domain above.
    view.addRotation(Vector(0._f, 0._f, _params.targetRotation), Vector(0._f));

    /*if (!_params.outputPath.empty()) {
        _body.saveToFile(_params.outputPath / Path("target.sph"));
    }*/
}

BodyView Presets::Collision::addImpactor(Storage& storage) {
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

    BodySettings impactorBody = _body;
    impactorBody.set(BodySettingsId::PARTICLE_COUNT, int(impactorParticleCnt))
        .set(BodySettingsId::BODY_CENTER, center)
        .set(BodySettingsId::BODY_VELOCITY, v_imp)
        .set(BodySettingsId::BODY_ANGULAR_VELOCITY, Vector(0._f));
    if (_params.optimizeImpactor) {
        // check that we don't use it for similar bodies
        ASSERT(_params.impactorRadius < 0.5_f * _params.targetRadius);
        impactorBody.set(BodySettingsId::STRESS_TENSOR_MIN, LARGE).set(BodySettingsId::DAMAGE_MIN, LARGE);
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

    /*if (!_params.outputPath.empty()) {
        impactorBody.saveToFile(_params.outputPath / Path("impactor.sph"));
    }*/
}

Vector Presets::Collision::getImpactPoint() const {
    const Float impactorDistance = _params.targetRadius + _params.impactorRadius;
    const Float x = impactorDistance * cos(_params.impactAngle);
    const Float y = impactorDistance * sin(_params.impactAngle);
    return Vector(x, y, 0._f);
}

Presets::Satellite::Satellite(ISolver& solver,
    const RunSettings& settings,
    const BodySettings& body,
    const SatelliteParams& params)
    : _ic(solver, settings)
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

NAMESPACE_SPH_END
