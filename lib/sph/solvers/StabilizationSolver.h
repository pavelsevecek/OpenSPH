#pragma once

/// \file StabilizationSolver.h
/// \brief Helper solver used to converge into stable initial conditions.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "quantities/IMaterial.h"
#include "system/Factory.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Helper solver used to converge into stable initial conditions.
///
/// It is a wrapper of another solver (assumed SPH solver, but it can be theoretically anything).
/// \ref StabilizationSolver forwards calls to the underlying solver, but it also damps particle velocities
/// and additionally resets all material fracture every timestep, provided the underlying solver uses
/// fracture.
class StabilizationSolver : public ISolver {
private:
    AutoPtr<ISolver> solver;

    /// Time range of the phase
    Interval timeRange;

    /// Velocity damping constant
    Float delta;

public:
    StabilizationSolver(const Interval timeRange, const Float delta, AutoPtr<ISolver>&& solver)
        : solver(std::move(solver))
        , timeRange(timeRange)
        , delta(delta) {}

    StabilizationSolver(const RunSettings& settings, AutoPtr<ISolver>&& solver)
        : solver(std::move(solver)) {
        timeRange = settings.get<Interval>(RunSettingsId::RUN_TIME_RANGE);
        delta = settings.get<Float>(RunSettingsId::SPH_STABILIZATION_DAMPING);
    }

    StabilizationSolver(IScheduler& scheduler, const RunSettings& settings)
        : StabilizationSolver(settings, Factory::getSolver(scheduler, settings)) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
        solver->integrate(storage, stats);

        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        const Float dt = stats.getOr<Float>(StatisticsId::TIMESTEP_VALUE, 0.01_f);

        if (t > timeRange.upper()) {
            return;
        }

        // damp velocities
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
        for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
            MaterialView mat = storage.getMaterial(matId);
            const Vector center = mat->getParam<Vector>(BodySettingsId::BODY_CENTER);
            const Vector v0 = mat->getParam<Vector>(BodySettingsId::BODY_VELOCITY);
            const Vector omega0 = mat->getParam<Vector>(BodySettingsId::BODY_SPIN_RATE);
            for (Size i : mat.sequence()) {
                // gradually decrease the delta
                const Float factor =
                    1._f + lerp(delta * dt, 0._f, (t - timeRange.lower()) / timeRange.size());

                // if the body is moving and/or rotating, we have to damp the velocities in co-moving frame
                // rather than in world frame, otherwise we would slow the whole body down and lose (angular)
                // momentum.
                const Vector v_local = v0 + cross(omega0, r[i] - (center + v0 * t));
                // we have to dump only the deviation of velocities, not the initial rotation!
                v[i] = (v[i] - v_local) / factor + v_local;
                ASSERT(isReal(v[i]));
            }
        }

        if (storage.has(QuantityId::DAMAGE)) {
            // reset both the damage and its derivative
            ArrayView<Float> D, dD;
            tie(D, dD) = storage.getAll<Float>(QuantityId::DAMAGE);
            for (Size i = 0; i < D.size(); ++i) {
                D[i] = dD[i] = 0._f;
            }
        }

        if (storage.has(QuantityId::STRESS_REDUCING)) {
            ArrayView<Float> Y = storage.getValue<Float>(QuantityId::STRESS_REDUCING);
            for (Size i = 0; i < Y.size(); ++i) {
                Y[i] = 1._f;
            }
        }
    }

    virtual void collide(Storage& storage, Statistics& stats, const Float dt) override {
        // no damping needed here, just forward the call
        solver->collide(storage, stats, dt);
    }

    virtual void create(Storage& storage, IMaterial& material) const override {
        solver->create(storage, material);
    }
};

NAMESPACE_SPH_END
