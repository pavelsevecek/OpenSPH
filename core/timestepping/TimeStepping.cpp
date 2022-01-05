#include "timestepping/TimeStepping.h"
#include "quantities/Attractor.h"
#include "quantities/IMaterial.h"
#include "quantities/Iterate.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/Scheduler.h"
#include "timestepping/ISolver.h"
#include "timestepping/TimeStepCriterion.h"

NAMESPACE_SPH_BEGIN

ITimeStepping::ITimeStepping(const SharedPtr<Storage>& storage,
    const RunSettings& settings,
    AutoPtr<ITimeStepCriterion>&& criterion)
    : storage(storage)
    , criterion(std::move(criterion)) {
    timeStep = settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);
    maxTimeStep = settings.get<Float>(RunSettingsId::TIMESTEPPING_MAX_TIMESTEP);
    saveParticleTimeSteps = settings.get<bool>(RunSettingsId::SAVE_PARTICLE_TIMESTEPS);

    if (saveParticleTimeSteps) {
        storage->insert<Float>(QuantityId::TIME_STEP, OrderEnum::ZERO, LARGE);
        storage->insert<Size>(QuantityId::TIME_STEP_CRITERION, OrderEnum::ZERO, 0);
    }
}

ITimeStepping::ITimeStepping(const SharedPtr<Storage>& storage, const RunSettings& settings)
    : ITimeStepping(storage, settings, Factory::getTimeStepCriterion(settings)) {}

ITimeStepping::~ITimeStepping() = default;

void ITimeStepping::step(IScheduler& scheduler, ISolver& solver, Statistics& stats) {
    Timer timer;
    // drift attractors
    for (Attractor& a : storage->getAttractors()) {
        a.position += 0.5_f * a.velocity * timeStep;
    }

    // compute partiles
    this->stepParticles(scheduler, solver, stats);

    // kick & drift attractors
    for (Attractor& a : storage->getAttractors()) {
        a.velocity += a.acceleration * timeStep;
        a.position += 0.5_f * a.velocity * timeStep;
    }

    // update time step
    CriterionId criterionId = CriterionId::INITIAL_VALUE;
    if (criterion) {
        Array<TimeStep> dts;
        if (saveParticleTimeSteps) {
            dts.resizeAndSet(storage->getParticleCnt(), TimeStep{ LARGE, CriterionId::MAXIMAL_VALUE });
        }
        const TimeStep result = criterion->compute(scheduler, *storage, maxTimeStep, stats, dts);
        timeStep = result.value;
        criterionId = result.id;
        if (saveParticleTimeSteps) {
            ArrayView<Float> values = storage->getValue<Float>(QuantityId::TIME_STEP);
            ArrayView<Size> critIds = storage->getValue<Size>(QuantityId::TIME_STEP_CRITERION);
            parallelFor(scheduler, 0, dts.size(), [&values, &critIds, &dts](const Size i) {
                values[i] = dts[i].value;
                critIds[i] = Size(dts[i].id);
            });
        }
    }
    stats.set(StatisticsId::TIMESTEP_VALUE, timeStep);
    stats.set(StatisticsId::TIMESTEP_CRITERION, criterionId);
    stats.set(StatisticsId::TIMESTEP_ELAPSED, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

//-----------------------------------------------------------------------------------------------------------
// Helper functions for stepping
//-----------------------------------------------------------------------------------------------------------

template <typename TFunc>
static void stepFirstOrder(Storage& storage, IScheduler& scheduler, const TFunc& stepper) {

    // note that derivatives are not advanced in time, but cannot be const as they might be clamped
    auto process = [&](const QuantityId id, auto& x, auto& dx) {
        SPH_ASSERT(x.size() == dx.size());

        parallelFor(scheduler, 0, x.size(), [&](const Size i) INL {
            stepper(x[i], asConst(dx[i]));
            const Interval range = storage.getMaterialOfParticle(i)->range(id);
            if (range != Interval::unbounded()) {
                tie(x[i], dx[i]) = clampWithDerivative(x[i], dx[i], range);
            }
        });
    };

    iterate<VisitorEnum::FIRST_ORDER>(storage, SEQUENTIAL, process);
}

template <typename TFunc>
static void stepSecondOrder(Storage& storage, IScheduler& scheduler, const TFunc& stepper) {

    auto process = [&](const QuantityId id, auto& r, auto& v, const auto& dv) {
        SPH_ASSERT(r.size() == v.size() && r.size() == dv.size());

        parallelFor(scheduler, 0, r.size(), [&](const Size i) INL {
            stepper(r[i], v[i], dv[i]);
            /// \todo optimize gettings range of materials (same in derivativecriterion for minimals)
            const Interval range = storage.getMaterialOfParticle(i)->range(id);
            if (range != Interval::unbounded()) {
                tie(r[i], v[i]) = clampWithDerivative(r[i], v[i], range);
            }
        });
    };

    iterate<VisitorEnum::SECOND_ORDER>(storage, SEQUENTIAL, process);
}

template <typename TFunc>
static void stepFirstOrder(Storage& storage1,
    Storage& storage2,
    IScheduler& scheduler,
    const TFunc& stepper) {

    auto processPair = [&](QuantityId id, auto& px, auto& pdx, const auto& cx, const auto& cdx) {
        SPH_ASSERT(px.size() == pdx.size());
        SPH_ASSERT(cdx.size() == px.size());
        SPH_ASSERT(cx.empty());

        parallelFor(scheduler, 0, px.size(), [&](const Size i) {
            stepper(px[i], pdx[i], cdx[i]);

            const Interval range = storage1.getMaterialOfParticle(i)->range(id);
            if (range != Interval::unbounded()) {
                tie(px[i], pdx[i]) = clampWithDerivative(px[i], pdx[i], range);
            }
        });
    };

    iteratePair<VisitorEnum::FIRST_ORDER>(storage1, storage2, processPair);
}

/// \todo remove code duplication using if constexpr
template <typename TFunc>
static void stepPairFirstOrder(Storage& storage1,
    Storage& storage2,
    IScheduler& scheduler,
    const TFunc& stepper) {

    auto processPair = [&](QuantityId id, auto& px, auto& pdx, const auto& cx, const auto& cdx) {
        SPH_ASSERT(px.size() == pdx.size());
        SPH_ASSERT(cdx.size() == px.size());
        SPH_ASSERT(cx.size() == cdx.size());

        parallelFor(scheduler, 0, px.size(), [&](const Size i) {
            stepper(px[i], pdx[i], cx[i], cdx[i]);

            const Interval range = storage1.getMaterialOfParticle(i)->range(id);
            if (range != Interval::unbounded()) {
                tie(px[i], pdx[i]) = clampWithDerivative(px[i], pdx[i], range);
            }
        });
    };

    iteratePair<VisitorEnum::FIRST_ORDER>(storage1, storage2, processPair);
}

template <typename TFunc>
static void stepSecondOrder(Storage& storage1,
    Storage& storage2,
    IScheduler& scheduler,
    const TFunc& stepper) {

    auto processPair = [&](QuantityId id,
                           auto& pr,
                           auto& pv,
                           const auto& pdv,
                           const auto& cr,
                           const auto& cv,
                           const auto& cdv) {
        SPH_ASSERT(pr.size() == pv.size() && pr.size() == pdv.size());
        SPH_ASSERT(cdv.size() == pr.size());
        SPH_ASSERT(cr.empty());
        SPH_ASSERT(cv.empty());

        parallelFor(scheduler, 0, pr.size(), [&](const Size i) {
            stepper(pr[i], pv[i], pdv[i], cdv[i]);

            const Interval range = storage1.getMaterialOfParticle(i)->range(id);
            if (range != Interval::unbounded()) {
                tie(pr[i], pv[i]) = clampWithDerivative(pr[i], pv[i], range);
            }
        });
    };

    iteratePair<VisitorEnum::SECOND_ORDER>(storage1, storage2, processPair);
}

template <typename TFunc>
static void stepPairSecondOrder(Storage& storage1,
    Storage& storage2,
    IScheduler& scheduler,
    const TFunc& stepper) {

    auto processPair = [&](QuantityId id,
                           auto& pr,
                           auto& pv,
                           const auto& pdv,
                           const auto& cr,
                           const auto& cv,
                           const auto& cdv) {
        SPH_ASSERT(pr.size() == pv.size() && pr.size() == pdv.size());
        SPH_ASSERT(cdv.size() == pr.size());
        SPH_ASSERT(cr.size() == cdv.size());
        SPH_ASSERT(cv.size() == cdv.size());

        parallelFor(scheduler, 0, pr.size(), [&](const Size i) {
            stepper(pr[i], pv[i], pdv[i], cr[i], cv[i], cdv[i]);

            const Interval range = storage1.getMaterialOfParticle(i)->range(id);
            if (range != Interval::unbounded()) {
                tie(pr[i], pv[i]) = clampWithDerivative(pr[i], pv[i], range);
            }
        });
    };

    iteratePair<VisitorEnum::SECOND_ORDER>(storage1, storage2, processPair);
}

//-----------------------------------------------------------------------------------------------------------
// EulerExplicit implementation
//-----------------------------------------------------------------------------------------------------------

void EulerExplicit::stepParticles(IScheduler& scheduler, ISolver& solver, Statistics& stats) {
    VERBOSE_LOG

    // clear derivatives from previous timestep
    storage->zeroHighestDerivatives(scheduler);

    // compute derivatives
    solver.integrate(*storage, stats);

    PROFILE_SCOPE("EulerExplicit::step")
    const Float dt = timeStep;

    // advance velocities
    stepSecondOrder(*storage, scheduler, [dt](auto& UNUSED(r), auto& v, const auto& dv) INL { //
        using Type = typename std::decay_t<decltype(v)>;
        v += Type(dv * dt);
    });
    // find positions and velocities after collision (at the beginning of the time step
    solver.collide(*storage, stats, timeStep);
    // advance positions
    stepSecondOrder(*storage, scheduler, [dt](auto& r, auto& v, const auto& UNUSED(dv)) INL { //
        using Type = typename std::decay_t<decltype(v)>;
        r += Type(v * dt);
    });

    // simply advance first order quanties
    stepFirstOrder(*storage, scheduler, [dt](auto& x, const auto& dx) INL { //
        using Type = typename std::decay_t<decltype(x)>;
        x += Type(dx * dt);
    });

    SPH_ASSERT(storage->isValid());
}

//-----------------------------------------------------------------------------------------------------------
// PredictorCorrector implementation
//-----------------------------------------------------------------------------------------------------------

PredictorCorrector::PredictorCorrector(const SharedPtr<Storage>& storage, const RunSettings& settings)
    : ITimeStepping(storage, settings) {
    SPH_ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced

    // we need to keep a copy of the highest derivatives (predictions)
    predictions = makeShared<Storage>(storage->clone(VisitorEnum::HIGHEST_DERIVATIVES));
    storage->addDependent(predictions);

    // clear derivatives before using them in step method
    storage->zeroHighestDerivatives(SEQUENTIAL);
}

PredictorCorrector::~PredictorCorrector() = default;

void PredictorCorrector::makePredictions(IScheduler& scheduler) {
    PROFILE_SCOPE("PredictorCorrector predictions")
    const Float dt = timeStep;
    const Float dt2 = 0.5_f * sqr(dt);
    /// \todo this is currently incompatible with NBodySolver, because we advance positions by 0.5 adt^2 ...
    stepSecondOrder(*storage, scheduler, [dt, dt2](auto& r, auto& v, const auto& dv) INL {
        using Type = typename std::decay_t<decltype(v)>;
        r += Type(v * dt + dv * dt2);
        v += Type(dv * dt);
    });
    stepFirstOrder(*storage, scheduler, [dt](auto& x, const auto& dx) INL { //
        using Type = typename std::decay_t<decltype(x)>;
        x += Type(dx * dt);
    });
}

void PredictorCorrector::makeCorrections(IScheduler& scheduler) {
    PROFILE_SCOPE("PredictorCorrector corrections");
    const Float dt = timeStep;
    const Float dt2 = 0.5_f * sqr(dt);
    constexpr Float a = 1._f / 3._f;
    constexpr Float b = 0.5_f;

    stepSecondOrder(*storage,
        *predictions,
        scheduler,
        [a, b, dt, dt2](auto& pr, auto& pv, const auto& pdv, const auto& cdv) {
            using Type = typename std::decay_t<decltype(pr)>;
            pr -= Type(a * (cdv - pdv) * dt2);
            pv -= Type(b * (cdv - pdv) * dt);
        });

    stepFirstOrder(*storage, *predictions, scheduler, [dt](auto& px, const auto& pdx, const auto& cdx) {
        using Type = typename std::decay_t<decltype(px)>;
        px -= Type(0.5_f * (cdx - pdx) * dt);
    });
}

void PredictorCorrector::stepParticles(IScheduler& scheduler, ISolver& solver, Statistics& stats) {
    VERBOSE_LOG

    // make predictions
    this->makePredictions(scheduler);

    // save derivatives from predictions
    storage->swap(*predictions, VisitorEnum::HIGHEST_DERIVATIVES);

    // clear derivatives
    storage->zeroHighestDerivatives(scheduler);

    // compute derivatives
    solver.integrate(*storage, stats);
    SPH_ASSERT(storage->getParticleCnt() == predictions->getParticleCnt(),
        storage->getParticleCnt(),
        predictions->getParticleCnt());

    // make corrections
    this->makeCorrections(scheduler);

    SPH_ASSERT(storage->isValid());
}

//-----------------------------------------------------------------------------------------------------------
// Leapfrog implementation
//-----------------------------------------------------------------------------------------------------------

void LeapFrog::stepParticles(IScheduler& scheduler, ISolver& solver, Statistics& stats) {
    VERBOSE_LOG

    // move positions by half a timestep (drift)
    const Float dt = timeStep;
    solver.collide(*storage, stats, 0.5_f * dt);
    stepSecondOrder(*storage, scheduler, [dt](auto& r, const auto& v, const auto& UNUSED(dv)) INL { //
        using Type = typename std::decay_t<decltype(v)>;
        r += Type(v * 0.5_f * dt);
    });

    // compute the derivatives
    storage->zeroHighestDerivatives(scheduler);
    solver.integrate(*storage, stats);

    // integrate first-order quantities as in Euler
    /// \todo this is not LeapFrog !
    stepFirstOrder(*storage, scheduler, [dt](auto& x, const auto& dx) INL { //
        using Type = typename std::decay_t<decltype(x)>;
        x += Type(dx * dt);
    });


    // move velocities by full timestep (kick)
    stepSecondOrder(*storage, scheduler, [dt](auto& UNUSED(r), auto& v, const auto& dv) INL { //
        using Type = typename std::decay_t<decltype(v)>;
        v += Type(dv * dt);
    });

    // evaluate collisions
    solver.collide(*storage, stats, 0.5_f * dt);

    // move positions by another half timestep (drift)
    stepSecondOrder(*storage, scheduler, [dt](auto& r, auto& v, const auto& UNUSED(dv)) INL { //
        using Type = typename std::decay_t<decltype(v)>;
        r += Type(v * 0.5_f * dt);
    });

    SPH_ASSERT(storage->isValid());
}

//-----------------------------------------------------------------------------------------------------------
// RungeKutta implementation
//-----------------------------------------------------------------------------------------------------------

RungeKutta::RungeKutta(const SharedPtr<Storage>& storage, const RunSettings& settings)
    : ITimeStepping(storage, settings) {
    SPH_ASSERT(storage->getQuantityCnt() > 0); // quantities must already been emplaced
    k1 = makeShared<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k2 = makeShared<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k3 = makeShared<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));
    k4 = makeShared<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));

    storage->addDependent(k1);
    storage->addDependent(k2);
    storage->addDependent(k3);
    storage->addDependent(k4);

    // clear derivatives before using them in step method
    storage->zeroHighestDerivatives(SEQUENTIAL);
}

RungeKutta::~RungeKutta() = default;

void RungeKutta::integrateAndAdvance(ISolver& solver,
    Statistics& stats,
    Storage& k,
    const Float m,
    const Float n) {
    k.zeroHighestDerivatives(SEQUENTIAL);
    solver.integrate(k, stats);
    iteratePair<VisitorEnum::FIRST_ORDER>(
        k, *storage, [&](const QuantityId UNUSED(id), auto& kv, auto& kdv, auto& v, auto&) {
            using Type = typename std::decay_t<decltype(kv)>::Type;
            for (Size i = 0; i < v.size(); ++i) {
                kv[i] += Type(kdv[i] * m * this->timeStep);
                v[i] += Type(kdv[i] * n * this->timeStep);
            }
        });
    iteratePair<VisitorEnum::SECOND_ORDER>(k,
        *storage,
        [&](const QuantityId UNUSED(id), auto& kv, auto& kdv, auto& kd2v, auto& v, auto& dv, auto&) {
            using Type = typename std::decay_t<decltype(kv)>::Type;
            for (Size i = 0; i < v.size(); ++i) {
                kv[i] += Type(kdv[i] * m * this->timeStep);
                kdv[i] += Type(kd2v[i] * m * this->timeStep);
                v[i] += Type(kdv[i] * n * this->timeStep);
                dv[i] += Type(kd2v[i] * n * this->timeStep);
            }
        });
}

void RungeKutta::stepParticles(IScheduler& scheduler, ISolver& solver, Statistics& stats) {
    k1->zeroHighestDerivatives(scheduler);
    k2->zeroHighestDerivatives(scheduler);
    k3->zeroHighestDerivatives(scheduler);
    k4->zeroHighestDerivatives(scheduler);

    solver.integrate(*k1, stats);
    integrateAndAdvance(solver, stats, *k1, 0.5_f, 1._f / 6._f);
    // swap values of 1st order quantities and both values and 1st derivatives of 2nd order quantities
    k1->swap(*k2, VisitorEnum::STATE_VALUES);

    /// \todo derivatives of storage (original, not k1, ..., k4) are never used
    // compute k2 derivatives based on values computes in previous integration
    solver.integrate(*k2, stats);
    /// \todo at this point, I no longer need k1, we just need 2 auxiliary buffers
    integrateAndAdvance(solver, stats, *k2, 0.5_f, 1._f / 3._f);
    k2->swap(*k3, VisitorEnum::STATE_VALUES);

    solver.integrate(*k3, stats);
    integrateAndAdvance(solver, stats, *k3, 0.5_f, 1._f / 3._f);
    k3->swap(*k4, VisitorEnum::STATE_VALUES);

    solver.integrate(*k4, stats);

    iteratePair<VisitorEnum::FIRST_ORDER>(
        *storage, *k4, [this](const QuantityId UNUSED(id), auto& v, auto&, auto&, auto& kdv) {
            using Type = typename std::decay_t<decltype(v)>::Type;
            for (Size i = 0; i < v.size(); ++i) {
                v[i] += Type(this->timeStep / 6._f * kdv[i]);
            }
        });
    iteratePair<VisitorEnum::SECOND_ORDER>(*storage,
        *k4,
        [this](const QuantityId UNUSED(id), auto& v, auto& dv, auto&, auto&, auto& kdv, auto& kd2v) {
            using Type = typename std::decay_t<decltype(v)>::Type;
            for (Size i = 0; i < v.size(); ++i) {
                dv[i] += Type(this->timeStep / 6._f * kd2v[i]);
                v[i] += Type(this->timeStep / 6._f * kdv[i]);
            }
        });
}

//-----------------------------------------------------------------------------------------------------------
// ModifiedMidpointMethod implementation
//-----------------------------------------------------------------------------------------------------------

ModifiedMidpointMethod::ModifiedMidpointMethod(const SharedPtr<Storage>& storage, const RunSettings& settings)
    : ITimeStepping(storage, settings) {
    n = settings.get<int>(RunSettingsId::TIMESTEPPING_MIDPOINT_COUNT);
    mid = makeShared<Storage>(storage->clone(VisitorEnum::ALL_BUFFERS));

    // connect the storage in the other direction, as we call solver with mid
    mid->addDependent(storage);
}

void ModifiedMidpointMethod::stepParticles(IScheduler& scheduler, ISolver& solver, Statistics& stats) {
    const Float h = timeStep / n; // current substep

    solver.collide(*storage, stats, h);
    // do first (half)step using current derivatives, save values to mid
    stepPairSecondOrder(*mid,
        *storage,
        scheduler,
        [h](auto& pr, auto& pv, const auto&, const auto& cr, const auto& cv, const auto& cdv) INL {
            using Type = typename std::decay_t<decltype(cv)>;
            pv = Type(cv + h * cdv);
            pr = Type(cr + h * cv);
            SPH_ASSERT(isReal(pv) && isReal(pr));
        });
    stepPairFirstOrder(*mid,
        *storage,
        scheduler,
        [h](auto& px, const auto& UNUSED(pdx), const auto& cx, const auto& cdx) INL { //
            using Type = typename std::decay_t<decltype(cx)>;
            px = Type(cx + h * cdx);
            SPH_ASSERT(isReal(px));
        });

    mid->zeroHighestDerivatives(scheduler);
    // evaluate the derivatives, using the advanced values
    solver.integrate(*mid, stats);

    // now mid is half-step ahead of the storage in both values and derivatives

    // do (n-1) steps, keeping mid half-step ahead of the storage
    for (Size iter = 0; iter < n - 1; ++iter) {
        solver.collide(*storage, stats, 2._f * h);
        stepPairSecondOrder(*storage,
            *mid,
            scheduler,
            [h](auto& pr,
                auto& pv,
                const auto& UNUSED(pdv),
                const auto& UNUSED(cr),
                const auto& cv,
                const auto& cdv) INL {
                using Type = typename std::decay_t<decltype(pr)>;
                pv += Type(2._f * h * cdv);
                pr += Type(2._f * h * cv);
                SPH_ASSERT(isReal(pv) && isReal(pr));
            });
        stepPairFirstOrder(*storage,
            *mid,
            scheduler,
            [h](auto& px, const auto& UNUSED(pdx), const auto& UNUSED(cx), const auto& cdx) INL { //
                using Type = typename std::decay_t<decltype(px)>;
                px += Type(2._f * h * cdx);
                SPH_ASSERT(isReal(px));
            });
        storage->swap(*mid, VisitorEnum::ALL_BUFFERS);
        mid->zeroHighestDerivatives(scheduler);
        solver.integrate(*mid, stats);
    }

    // last step
    solver.collide(*storage, stats, h);
    stepPairSecondOrder(*storage,
        *mid,
        scheduler,
        [h](auto& pr, auto& pv, const auto& UNUSED(pdv), auto& cr, const auto& cv, const auto& cdv) INL {
            using Type = typename std::decay_t<decltype(pr)>;
            pv = Type(0.5_f * (pv + cv + h * cdv));
            pr = Type(0.5_f * (pr + cr + h * cv));
            SPH_ASSERT(isReal(pv) && isReal(pr));
        });
    stepPairFirstOrder(*storage,
        *mid,
        scheduler,
        [h](auto& px, const auto& UNUSED(pdx), const auto& cx, const auto& cdx) INL {
            using Type = typename std::decay_t<decltype(px)>;
            px = Type(0.5_f * (px + cx + h * cdx));
            SPH_ASSERT(isReal(px));
        });
}

//-----------------------------------------------------------------------------------------------------------
// BulirschStoer implementation
//-----------------------------------------------------------------------------------------------------------

constexpr Size BS_SIZE = 9;
const StaticArray<Size, BS_SIZE> BS_STEPS = { 2, 4, 6, 8, 10, 12, 14, 16, 18 };

template <typename T, Size Dim1, Size Dim2>
using TwoDimensionalStaticArray = StaticArray<StaticArray<T, Dim1>, Dim2>;

BulirschStoer::BulirschStoer(const SharedPtr<Storage>& storage, const RunSettings& settings)
    : ITimeStepping(storage, settings) {
    eps = settings.get<Float>(RunSettingsId::TIMESTEPPING_BS_ACCURACY);

    /*const Float safe1 = 0.25_f;
    const Float safe2 = 0.7_f;*/

    // compute the work coefficients A_i
    StaticArray<Float, BS_SIZE + 1> A;
    A[0] = BS_STEPS[0] + 1;
    for (Size i = 0; i < BS_SIZE; ++i) {
        A[i + 1] = A[i] + BS_STEPS[i + 1];
    }

    // compute the correction factors alpha(k, q)
    TwoDimensionalStaticArray<Float, BS_SIZE, BS_SIZE> alpha;

#ifdef SPH_DEBUG
    for (auto& row : alpha) {
        row.fill(NAN);
    }
#endif

    for (Size q = 0; q < BS_SIZE; ++q) {
        alpha[q][q] = 1._f;

        for (Size k = 0; k < q; ++k) {
            alpha[k][q] = (A[k + 1] - A[q + 1]) / std::pow(eps, (2 * k + 1) * (A[q + 1] - A[0] + 1));
        }
    }

    // determine the optimal row number of convergence
    Size rowNumber = 0;
    (void)rowNumber;
    for (Size i = 1; i < BS_SIZE; ++i) {
        SPH_ASSERT(isReal(alpha[i - 1][i]));
        if (A[i + 1] > A[i] * alpha[i - 1][i]) {
            rowNumber = i;
        }
    }
    SPH_ASSERT(rowNumber > 0._f);
}

void BulirschStoer::stepParticles(IScheduler& UNUSED(scheduler), ISolver& solver, Statistics& stats) {
    (void)solver;
    (void)stats;
}

NAMESPACE_SPH_END
