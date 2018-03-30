#include "sph/initial/Presets.h"
#include "objects/geometry/Domain.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

Presets::Collision::Collision(ISolver& solver,
    const RunSettings& settings,
    const BodySettings& body,
    const CollisionParams& params)
    : _ic(solver, settings)
    , _body(body)
    , _params(params) {
    ASSERT(params.impactAngle >= 0._f && params.impactAngle < 2._f * PI);
    ASSERT(params.impactSpeed > 0._f);
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
    view.addRotation(Vector(0._f, 0._f, _params.targetRotation), Vector(0._f));

    if (!_params.outputPath.empty()) {
        _body.saveToFile(_params.outputPath / Path("target.sph"));
    }
}

void Presets::Collision::addImpactor(Storage& storage) {
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

    const Float impactorDistance = _params.targetRadius + _params.impactorRadius;
    // move impactor by 6h so that there is no overlap
    const Float x = impactorDistance * cos(_params.impactAngle) + 6._f * h;
    const Float y = impactorDistance * sin(_params.impactAngle);
    const Vector center(x, y, 0._f);
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
    _ic.addMonolithicBody(storage, domain, impactorBody).addVelocity(v_imp);

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
    }

    if (!_params.outputPath.empty()) {
        impactorBody.saveToFile(_params.outputPath / Path("impactor.sph"));
    }
}

Vector Presets::Collision::getImpactPoint() const {
    const Float impactorDistance = _params.targetRadius + _params.impactorRadius;
    const Float x = impactorDistance * cos(_params.impactAngle);
    const Float y = impactorDistance * sin(_params.impactAngle);
    return Vector(x, y, 0._f);
}

NAMESPACE_SPH_END
