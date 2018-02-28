#pragma once

/// \file Present.h
/// \brief Problem-specific initial conditions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/geometry/Domain.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

namespace Presets {
struct CollisionParams {
    /// Radius of the parent body in meters
    Float targetRadius;

    /// Radius of the projectile in meters
    Float projectileRadius;

    /// Impact speed in m/s
    Float impactSpeed;

    /// Impact angle, i.e. angle between velocity vector and normal at the impact point
    Float impactAngle;

    /// Angular frequency of the target
    Float targetRotation = 0._f;

    /// Number of target particles
    Size targetParticleCnt = 100000;

    /// Minimal number of particles per body
    Size minParticleCnt = 100;

    /// Number of impactor particles.
    ///
    /// If unused (value is NOTHING), the number of particles is automatically computed based on the
    /// number of target particles and the ratio of target radius to projectile radius.
    Optional<Size> impactorParticleCntOverride = NOTHING;

    /// Path to the output directory; is set, parameters of target and impactor are saved there.
    Path outputPath;

    /// Function specifying particle concentration inside the target. If not specified, particles are
    /// spaced homogeneously.
    Function<Float(const Vector& r)> concentration;
};


class Collision : public Noncopyable {
private:
    InitialConditions _ic;
    BodySettings _body;
    CollisionParams _params;

public:
    Collision(ISolver& solver,
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

    void addTarget(Storage& storage) {
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

    void addImpactor(Storage& storage) {
        const Float targetDensity = _params.targetParticleCnt / pow<3>(_params.targetRadius);
        const Float h = 1.f / root<3>(targetDensity);
        ASSERT(h > 0._f);

        Size impactorParticleCnt;
        if (_params.impactorParticleCntOverride) {
            impactorParticleCnt = _params.impactorParticleCntOverride.value();
        } else {
            impactorParticleCnt =
                max<int>(_params.minParticleCnt, targetDensity * pow<3>(_params.projectileRadius));
        }

        const Float impactorDistance = _params.targetRadius + _params.projectileRadius;
        // move impactor by 3h so that there is no overlap
        const Float x = impactorDistance * cos(_params.impactAngle) + 3._f * h;
        const Float y = impactorDistance * sin(_params.impactAngle);
        const Vector center(x, y, 0._f);
        const Vector v_imp(-_params.impactSpeed, 0._f, 0._f);

        BodySettings impactorBody = _body;
        impactorBody.set(BodySettingsId::PARTICLE_COUNT, int(impactorParticleCnt))
            .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE)
            .set(BodySettingsId::DAMAGE_MIN, LARGE)
            .set(BodySettingsId::BODY_CENTER, center)
            .set(BodySettingsId::BODY_VELOCITY, v_imp)
            .set(BodySettingsId::BODY_ANGULAR_VELOCITY, Vector(0._f));

        SphericalDomain domain(center, _params.projectileRadius);
        _ic.addMonolithicBody(storage, domain, impactorBody).addVelocity(v_imp);

        if (!_params.outputPath.empty()) {
            impactorBody.saveToFile(_params.outputPath / Path("impactor.sph"));
        }
    }

    Vector getImpactPoint() const {
        const Float impactorDistance = _params.targetRadius + _params.projectileRadius;
        const Float x = impactorDistance * cos(_params.impactAngle);
        const Float y = impactorDistance * sin(_params.impactAngle);
        return Vector(x, y, 0._f);
    }
};
} // namespace Presets

NAMESPACE_SPH_END
