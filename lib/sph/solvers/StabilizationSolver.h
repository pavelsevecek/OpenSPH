#pragma once

/// \file StabilizationSolver.h
/// \brief Helper solver used to converge into stable initial conditions.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "post/Analysis.h"
#include "quantities/IMaterial.h"
#include "sph/boundary/Boundary.h"
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

    struct BodyData {
        Vector center;

        Vector omega;
    };

    Optional<BodyData> data;


public:
    StabilizationSolver(const Interval timeRange, const Float delta, AutoPtr<ISolver>&& solver)
        : solver(std::move(solver))
        , timeRange(timeRange)
        , delta(delta) {}

    StabilizationSolver(const RunSettings& settings, AutoPtr<ISolver>&& solver)
        : solver(std::move(solver)) {
        timeRange = Interval(settings.get<Float>(RunSettingsId::RUN_START_TIME),
            settings.get<Float>(RunSettingsId::RUN_END_TIME));
        delta = settings.get<Float>(RunSettingsId::SPH_STABILIZATION_DAMPING);
    }

    StabilizationSolver(IScheduler& scheduler, const RunSettings& settings, AutoPtr<IBoundaryCondition>&& bc)
        : StabilizationSolver(settings, Factory::getSolver(scheduler, settings, std::move(bc))) {}

    virtual void integrate(Storage& storage, Statistics& stats) override {
        solver->integrate(storage, stats);

        const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
        const Float dt = stats.getOr<Float>(StatisticsId::TIMESTEP_VALUE, 0.01_f);

        if (!data) {
            computeBodyData(storage);
        }
        ASSERT(data);

        // damp velocities
        ArrayView<Vector> r, v, dv;
        tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

        for (Size i = 0; i < r.size(); ++i) {
            // gradually decrease the delta
            const Float factor = 1._f + lerp(delta * dt, 0._f, (t - timeRange.lower()) / timeRange.size());

            // if the body is moving and/or rotating, we have to damp the velocities in co-moving frame
            // rather than in world frame, otherwise we would slow the whole body down and lose (angular)
            // momentum.
            const Vector v_local = cross(data->omega, r[i] - data->center);

            // we have to dump only the deviation of velocities, not the initial rotation!
            v[i] = (v[i] - v_local) / factor + v_local;
            v[i][H] = 0._f;
            ASSERT(isReal(v[i]));
        }

        if (storage.has(QuantityId::DAMAGE)) {
            // reset both the damage and its derivative
            ArrayView<Float> D, dD;
            tie(D, dD) = storage.getAll<Float>(QuantityId::DAMAGE);
            const Float d0 = storage.getMaterial(0)->getParam<Float>(BodySettingsId::DAMAGE);
            for (Size i = 0; i < D.size(); ++i) {
                dD[i] = 0._f;
                D[i] = d0;
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

private:
    void computeBodyData(const Storage& storage) {
        data.emplace();

        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
        ArrayView<const Vector> v = storage.getDt<Vector>(QuantityId::POSITION);
        ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
        data->center = Post::getCenterOfMass(m, r);
        data->omega = Post::getAngularFrequency(m, r, v);
    }
};

NAMESPACE_SPH_END
