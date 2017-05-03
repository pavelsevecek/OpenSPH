#include "timestepping/TimeStepping.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Iterate.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "timestepping/AbstractSolver.h"
#include "timestepping/TimeStepCriterion.h"

NAMESPACE_SPH_BEGIN


Abstract::TimeStepping::TimeStepping(const std::shared_ptr<Storage>& storage, const RunSettings& settings)
    : storage(storage) {
    dt = settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);
    maxdt = settings.get<Float>(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP);
    adaptiveStep = Factory::getTimeStepCriterion(settings);
}

Abstract::TimeStepping::~TimeStepping() = default;

void Abstract::TimeStepping::step(Abstract::Solver& solver, Statistics& stats) {
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

void EulerExplicit::stepImpl(Abstract::Solver& solver, Statistics& stats) {
    MEASURE_SCOPE("EulerExplicit::step");
    // clear derivatives from previous timestep
    this->storage->init();

    // compute derivatives
    solver.integrate(*this->storage, stats);

    PROFILE_SCOPE("EulerExplicit::step")
    // advance all 2nd-order quantities by current timestep, first values, then 1st derivatives
    iterate<VisitorEnum::SECOND_ORDER>(
        *this->storage, [this](const QuantityId id, auto& v, auto& dv, auto& d2v) {
            ArrayView<Size> matIds = storage->getValue<Size>(QuantityId::MATERIAL_IDX);
            for (Size i = 0; i < v.size(); ++i) {
                dv[i] += d2v[i] * this->dt;
                v[i] += dv[i] * this->dt;
                const Range range = storage->getRange(id, matIds[i]);
                if (!range.isEmpty()) {
                    v[i] = clamp(v[i], range);
                }
            }
        });
    iterate<VisitorEnum::FIRST_ORDER>(*this->storage, [this](const QuantityId id, auto& v, auto& dv) {
        ArrayView<Size> matIds = storage->getValue<Size>(QuantityId::MATERIAL_IDX);
        for (Size i = 0; i < v.size(); ++i) {
            v[i] += dv[i] * this->dt;
            const Range range = storage->getRange(id, matIds[i]);
            if (!range.isEmpty()) {
                v[i] = clamp(v[i], range);
            }
        }
    });
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// PredictorCorrector implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

PredictorCorrector::PredictorCorrector(const std::shared_ptr<Storage>& storage, const RunSettings& settings)
    : Abstract::TimeStepping(storage, settings) {
    ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced
    predictions = std::make_unique<Storage>(storage->clone(VisitorEnum::HIGHEST_DERIVATIVES));
    storage->init(); // clear derivatives before using them in step method
}

PredictorCorrector::~PredictorCorrector() = default;

void PredictorCorrector::makePredictions() {
    PROFILE_SCOPE("PredictorCorrector::predictions")
    const Float dt2 = 0.5_f * sqr(this->dt);
    iterate<VisitorEnum::SECOND_ORDER>(
        *this->storage, [this, dt2](const QuantityId id, auto& v, auto& dv, auto& d2v) {
            ArrayView<Size> matIds = storage->getValue<Size>(QuantityId::MATERIAL_IDX);
            for (Size i = 0; i < v.size(); ++i) {
                v[i] += dv[i] * this->dt + d2v[i] * dt2;
                dv[i] += d2v[i] * this->dt;
                const Range range = storage->getRange(id, matIds[i]);
                if (!range.isEmpty()) {
                    v[i] = clamp(v[i], range);
                }
            }
        });
    iterate<VisitorEnum::FIRST_ORDER>(*this->storage, [this](const QuantityId id, auto& v, auto& dv) {
        ArrayView<Size> matIds = storage->getValue<Size>(QuantityId::MATERIAL_IDX);
        for (Size i = 0; i < v.size(); ++i) {
            v[i] += dv[i] * this->dt;
            const Range range = storage->getRange(id, matIds[i]);
            if (!range.isEmpty()) {
                v[i] = clamp(v[i], range);
            }
        }
    });
}

void PredictorCorrector::makeCorrections() {
    PROFILE_SCOPE("PredictorCorrector::step   Corrections");
    const Float dt2 = 0.5_f * sqr(this->dt);
    // clang-format off
    iteratePair<VisitorEnum::SECOND_ORDER>(*this->storage, *this->predictions,
        [this, dt2](const QuantityId id, auto& pv, auto& pdv, auto& pd2v, auto& UNUSED(cv), auto& UNUSED(cdv), auto& cd2v) {
        ASSERT(pv.size() == pd2v.size());
        constexpr Float a = 1._f / 3._f;
        constexpr Float b = 0.5_f;
        ArrayView<Size> matIds = storage->getValue<Size>(QuantityId::MATERIAL_IDX);
        for (Size i = 0; i < pv.size(); ++i) {
            pv[i] -= a * (cd2v[i] - pd2v[i]) * dt2;
            pdv[i] -= b * (cd2v[i] - pd2v[i]) * this->dt;
            const Range range = storage->getRange(id, matIds[i]);
            if (!range.isEmpty()) {
                pv[i] = clamp(pv[i], range);
            }
        }
    });
    iteratePair<VisitorEnum::FIRST_ORDER>(*this->storage, *this->predictions,
        [this](const QuantityId id, auto& pv, auto& pdv, auto& UNUSED(cv), auto& cdv) {
        ASSERT(pv.size() == pdv.size());
        ArrayView<Size> matIds = storage->getValue<Size>(QuantityId::MATERIAL_IDX);
        for (Size i = 0; i < pv.size(); ++i) {
            pv[i] -= 0.5_f * (cdv[i] - pdv[i]) * this->dt;
            const Range range = storage->getRange(id, matIds[i]);
            if (!range.isEmpty()) {
                pv[i] = clamp(pv[i], range);
            }
        }
    });
    // clang-format on
}

void PredictorCorrector::stepImpl(Abstract::Solver& solver, Statistics& stats) {
    // make predictions
    this->makePredictions();
    // save derivatives from predictions
    this->storage->swap(*predictions, VisitorEnum::HIGHEST_DERIVATIVES);
    // clear derivatives
    this->storage->init();
    // compute derivative
    solver.integrate(*this->storage, stats);
    // make corrections
    this->makeCorrections();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// RungeKutta implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

RungeKutta::RungeKutta(const std::shared_ptr<Storage>& storage, const RunSettings& settings)
    : Abstract::TimeStepping(storage, settings) {
    ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced
    k1 = std::make_unique<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k2 = std::make_unique<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k3 = std::make_unique<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k4 = std::make_unique<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    storage->init(); // clear derivatives before using them in step method
}

RungeKutta::~RungeKutta() = default;

void RungeKutta::integrateAndAdvance(Abstract::Solver& solver,
    Statistics& stats,
    Storage& k,
    const float m,
    const float n) {
    solver.integrate(k, stats);
    iteratePair<VisitorEnum::FIRST_ORDER>(
        k, *this->storage, [&](const QuantityId UNUSED(id), auto& kv, auto& kdv, auto& v, auto&) {
            for (Size i = 0; i < v.size(); ++i) {
                kv[i] += kdv[i] * m * this->dt;
                v[i] += kdv[i] * n * this->dt;
            }
        });
    iteratePair<VisitorEnum::SECOND_ORDER>(k,
        *this->storage,
        [&](const QuantityId UNUSED(id), auto& kv, auto& kdv, auto& kd2v, auto& v, auto& dv, auto&) {
            for (Size i = 0; i < v.size(); ++i) {
                kv[i] += kdv[i] * m * this->dt;
                kdv[i] += kd2v[i] * m * this->dt;
                v[i] += kdv[i] * n * this->dt;
                dv[i] += kd2v[i] * n * this->dt;
            }
        });
}

void RungeKutta::stepImpl(Abstract::Solver& solver, Statistics& stats) {
    k1->init();
    k2->init();
    k3->init();
    k4->init();

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
        *this->storage, *k4, [this](const QuantityId UNUSED(id), auto& v, auto&, auto&, auto& kdv) {
            for (Size i = 0; i < v.size(); ++i) {
                v[i] += this->dt / 6.f * kdv[i];
            }
        });
    iteratePair<VisitorEnum::SECOND_ORDER>(*this->storage,
        *k4,
        [this](const QuantityId UNUSED(id), auto& v, auto& dv, auto&, auto&, auto& kdv, auto& kd2v) {
            for (Size i = 0; i < v.size(); ++i) {
                dv[i] += this->dt / 6.f * kd2v[i];
                v[i] += this->dt / 6.f * kdv[i];
            }
        });
}

NAMESPACE_SPH_END
