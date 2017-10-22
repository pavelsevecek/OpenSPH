#pragma once

/// \file Present.h
/// \brief Problem-specific initial conditions
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/geometry/Domain.h"
#include "objects/wrappers/SharedPtr.h"
#include "quantities/Storage.h"
#include "sph/initial/Initial.h"

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

        /// Angular frequency of the frame (corotating with the target)
        Float targetRotation = 0._f;

        /// Number of target particles
        Size targetParticleCnt = 100000;

        /// Minimal number of particles per body
        Size minParticleCnt = 100;

        /// Path to the output directory; is set, parameters of target and impactor are saved there.
        Path outputPath;
    };


    class Collision {
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
        }

        void addTarget(Storage& storage) {
            SphericalDomain domain(Vector(0._f), _params.targetRadius);
            _ic.addMonolithicBody(storage, domain, _body);

            if (!_params.outputPath.empty()) {
                _body.saveToFile(_params.outputPath / Path("target.sph"));
            }
        }

        void addImpactor(Storage& storage) {
            const Float targetDensity = _params.targetParticleCnt / pow<3>(_params.targetRadius);
            const Float h = 1.f / root<3>(targetDensity);
            ASSERT(h > 0._f);
            const Size impactorParticleCnt = targetDensity * pow<3>(_params.projectileRadius);

            const Float impactorDistance = _params.targetRadius + _params.projectileRadius;
            // move impactor by 3h so that there is no overlap
            const Float x = impactorDistance * cos(_params.impactAngle) + 3._f * h;
            const Float y = impactorDistance * sin(_params.impactAngle);

            BodySettings impactorBody = _body;
            impactorBody
                .set(BodySettingsId::PARTICLE_COUNT, max<int>(impactorParticleCnt, _params.minParticleCnt))
                .set(BodySettingsId::STRESS_TENSOR_MIN, LARGE)
                .set(BodySettingsId::DAMAGE_MIN, LARGE);

            SphericalDomain domain(Vector(x, y, 0._f), _params.projectileRadius);
            _ic.addMonolithicBody(storage, domain, impactorBody)
                .addVelocity(Vector(-_params.impactSpeed, 0._f, 0._f))
                .addRotation(
                    Vector(0._f, 0._f, _params.targetRotation), BodyView::RotationOrigin::FRAME_ORIGIN);

            if (!_params.outputPath.empty()) {
                impactorBody.saveToFile(_params.outputPath / Path("impactor.sph"));
            }
        }
    };
} // namespace Presets

NAMESPACE_SPH_END
