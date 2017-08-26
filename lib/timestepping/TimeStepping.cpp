#include "timestepping/TimeStepping.h"
#include "quantities/IMaterial.h"
#include "quantities/Iterate.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "timestepping/ISolver.h"
#include "timestepping/TimeStepCriterion.h"

NAMESPACE_SPH_BEGIN


ITimeStepping::ITimeStepping(const SharedPtr<Storage>& storage, const RunSettings& settings)
    : storage(storage) {
    dt = settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);
    maxdt = settings.get<Float>(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP);
    adaptiveStep = Factory::getTimeStepCriterion(settings);
}

ITimeStepping::~ITimeStepping() = default;

void ITimeStepping::step(ISolver& solver, Statistics& stats) {
    Timer timer;
    this->stepImpl(solver, stats);
    // update time step
    CriterionId criterion = CriterionId::INITIAL_VALUE;
    if (adaptiveStep) {
        tieToTuple(dt, criterion) = adaptiveStep->compute(*storage, maxdt, stats);
    }
    stats.set(StatisticsId::TIMESTEP_VALUE, dt);
    stats.set(StatisticsId::TIMESTEP_CRITERION, criterion);
    stats.set(StatisticsId::TIMESTEP_ELAPSED, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// EulerExplicit implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

void EulerExplicit::stepImpl(ISolver& solver, Statistics& stats) {
    // MEASURE_SCOPE("EulerExplicit::step");
    // clear derivatives from previous timestep
    storage->zeroHighestDerivatives();

    // compute derivatives
    solver.integrate(*storage, stats);

    PROFILE_SCOPE("EulerExplicit::step")
    // advance all 2nd-order quantities by current timestep, first values, then 1st derivatives
    iterate<VisitorEnum::SECOND_ORDER>(*storage, [this](const QuantityId id, auto& v, auto& dv, auto& d2v) {
        for (Size i = 0; i < v.size(); ++i) {
            dv[i] += d2v[i] * this->dt;
            v[i] += dv[i] * this->dt;
            /// \todo optimize gettings range of materials (same in derivativecriterion for minimals)
            const Interval range = storage->getMaterialOfParticle(i)->range(id);
            if (range != Interval::unbounded()) {
                tie(v[i], dv[i]) = clampWithDerivative(v[i], dv[i], range);
            }
        }
    });
    iterate<VisitorEnum::FIRST_ORDER>(*storage, [this](const QuantityId id, auto& v, auto& dv) {
        for (Size i = 0; i < v.size(); ++i) {
            v[i] += dv[i] * this->dt;
            const Interval range = storage->getMaterialOfParticle(i)->range(id);
            if (range != Interval::unbounded()) {
                tie(v[i], dv[i]) = clampWithDerivative(v[i], dv[i], range);
            }
        }
    });
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PredictorCorrector implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

PredictorCorrector::PredictorCorrector(const SharedPtr<Storage>& storage, const RunSettings& settings)
    : ITimeStepping(storage, settings) {
    ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced
    predictions = makeAuto<Storage>(storage->clone(VisitorEnum::HIGHEST_DERIVATIVES));
    storage->zeroHighestDerivatives(); // clear derivatives before using them in step method
}

PredictorCorrector::~PredictorCorrector() = default;

void PredictorCorrector::makePredictions() {
    PROFILE_SCOPE("PredictorCorrector::predictions")
    const Float dt2 = 0.5_f * sqr(this->dt);
    iterate<VisitorEnum::SECOND_ORDER>(
        *storage, [this, dt2](const QuantityId id, auto& v, auto& dv, auto& d2v) {
            parallelFor(0, v.size(), [&](const Size n1, const Size n2) INL {
                for (Size i = n1; i < n2; ++i) {
                    v[i] += dv[i] * this->dt + d2v[i] * dt2;
                    dv[i] += d2v[i] * this->dt;
                    /// \todo this probably wont change that much, we could cache it
                    const Interval range = storage->getMaterialOfParticle(i)->range(id);
                    if (range != Interval::unbounded()) {
                        tie(v[i], dv[i]) = clampWithDerivative(v[i], dv[i], range);
                    }
                }
            });
        });
    iterate<VisitorEnum::FIRST_ORDER>(*storage, [this](const QuantityId id, auto& v, auto& dv) {
        parallelFor(0, v.size(), [&](const Size n1, const Size n2) INL {
            for (Size i = n1; i < n2; ++i) {
                v[i] += dv[i] * this->dt;
                const Interval range = storage->getMaterialOfParticle(i)->range(id);
                if (range != Interval::unbounded()) {
                    tie(v[i], dv[i]) = clampWithDerivative(v[i], dv[i], range);
                }
            }
        });
    });
}

void PredictorCorrector::makeCorrections() {
    PROFILE_SCOPE("PredictorCorrector::step   Corrections");
    const Float dt2 = 0.5_f * sqr(this->dt);
    iteratePair<VisitorEnum::SECOND_ORDER>(*storage,
        *predictions,
        [this, dt2](const QuantityId id,
            auto& pv,
            auto& pdv,
            auto& pd2v,
            auto& UNUSED(cv),
            auto& UNUSED(cdv),
            auto& cd2v) {
            ASSERT(pv.size() == pd2v.size());
            constexpr Float a = 1._f / 3._f;
            constexpr Float b = 0.5_f;
            parallelFor(0, pv.size(), [&](const Size n1, const Size n2) {
                for (Size i = n1; i < n2; ++i) {
                    pv[i] -= a * (cd2v[i] - pd2v[i]) * dt2;
                    pdv[i] -= b * (cd2v[i] - pd2v[i]) * this->dt;
                    const Interval range = storage->getMaterialOfParticle(i)->range(id);
                    if (range != Interval::unbounded()) {
                        tie(pv[i], pdv[i]) = clampWithDerivative(pv[i], pdv[i], range);
                    }
                }
            });
        });
    iteratePair<VisitorEnum::FIRST_ORDER>(*storage,
        *predictions,
        [this](const QuantityId id, auto& pv, auto& pdv, auto& UNUSED(cv), auto& cdv) {
            ASSERT(pv.size() == pdv.size());
            parallelFor(0, pv.size(), [&](const Size n1, const Size n2) {
                for (Size i = n1; i < n2; ++i) {
                    pv[i] -= 0.5_f * (cdv[i] - pdv[i]) * this->dt;
                    const Interval range = storage->getMaterialOfParticle(i)->range(id);
                    if (range != Interval::unbounded()) {
                        tie(pv[i], pdv[i]) = clampWithDerivative(pv[i], pdv[i], range);
                    }
                }
            });
        });
}

void PredictorCorrector::stepImpl(ISolver& solver, Statistics& stats) {
    // make predictions
    this->makePredictions();

    // save derivatives from predictions
    storage->swap(*predictions, VisitorEnum::HIGHEST_DERIVATIVES);

    // clear derivatives
    storage->zeroHighestDerivatives();

    // compute derivatives
    solver.integrate(*storage, stats);

    // resize the correction storage, needed because solver might add or remove particles
    ASSERT(storage->getParticleCnt() > 0);
    predictions->resize(storage->getParticleCnt(), Storage::ResizeFlag::KEEP_EMPTY_UNCHANGED);

    // make corrections
    this->makeCorrections();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// RungeKutta implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

RungeKutta::RungeKutta(const SharedPtr<Storage>& storage, const RunSettings& settings)
    : ITimeStepping(storage, settings) {
    ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced
    k1 = makeAuto<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k2 = makeAuto<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k3 = makeAuto<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k4 = makeAuto<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    storage->zeroHighestDerivatives(); // clear derivatives before using them in step method
}

RungeKutta::~RungeKutta() = default;

void RungeKutta::integrateAndAdvance(ISolver& solver,
    Statistics& stats,
    Storage& k,
    const float m,
    const float n) {
    solver.integrate(k, stats);
    iteratePair<VisitorEnum::FIRST_ORDER>(
        k, *storage, [&](const QuantityId UNUSED(id), auto& kv, auto& kdv, auto& v, auto&) {
            for (Size i = 0; i < v.size(); ++i) {
                kv[i] += kdv[i] * m * this->dt;
                v[i] += kdv[i] * n * this->dt;
            }
        });
    iteratePair<VisitorEnum::SECOND_ORDER>(k,
        *storage,
        [&](const QuantityId UNUSED(id), auto& kv, auto& kdv, auto& kd2v, auto& v, auto& dv, auto&) {
            for (Size i = 0; i < v.size(); ++i) {
                kv[i] += kdv[i] * m * this->dt;
                kdv[i] += kd2v[i] * m * this->dt;
                v[i] += kdv[i] * n * this->dt;
                dv[i] += kd2v[i] * n * this->dt;
            }
        });
}

void RungeKutta::stepImpl(ISolver& solver, Statistics& stats) {
    k1->zeroHighestDerivatives();
    k2->zeroHighestDerivatives();
    k3->zeroHighestDerivatives();
    k4->zeroHighestDerivatives();

    solver.integrate(*k1, stats);
    integrateAndAdvance(solver, stats, *k1, 0.5_f, 1._f / 6._f);
    // swap values of 1st order quantities and both values and 1st derivatives of 2nd order quantities
    k1->swap(*k2, VisitorEnum::DEPENDENT_VALUES);

    /// \todo derivatives of storage (original, not k1, ..., k4) are never used
    // compute k2 derivatives based on values computes in previous integration
    solver.integrate(*k2, stats);
    /// \todo at this point, I no longer need k1, we just need 2 auxiliary buffers
    integrateAndAdvance(solver, stats, *k2, 0.5_f, 1._f / 3._f);
    k2->swap(*k3, VisitorEnum::DEPENDENT_VALUES);

    solver.integrate(*k3, stats);
    integrateAndAdvance(solver, stats, *k3, 0.5_f, 1._f / 3._f);
    k3->swap(*k4, VisitorEnum::DEPENDENT_VALUES);

    solver.integrate(*k4, stats);

    iteratePair<VisitorEnum::FIRST_ORDER>(
        *storage, *k4, [this](const QuantityId UNUSED(id), auto& v, auto&, auto&, auto& kdv) {
            for (Size i = 0; i < v.size(); ++i) {
                v[i] += this->dt / 6.f * kdv[i];
            }
        });
    iteratePair<VisitorEnum::SECOND_ORDER>(*storage,
        *k4,
        [this](const QuantityId UNUSED(id), auto& v, auto& dv, auto&, auto&, auto& kdv, auto& kd2v) {
            for (Size i = 0; i < v.size(); ++i) {
                dv[i] += this->dt / 6.f * kd2v[i];
                v[i] += this->dt / 6.f * kdv[i];
            }
        });
}

NAMESPACE_SPH_END
