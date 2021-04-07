#include "timestepping/TimeStepCriterion.h"
#include "io/Logger.h"
#include "quantities/IMaterial.h"
#include "quantities/Iterate.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

std::ostream& operator<<(std::ostream& stream, const CriterionId id) {
    switch (id) {
    case CriterionId::CFL_CONDITION:
        stream << "CFL condition";
        break;
    case CriterionId::ACCELERATION:
        stream << "Acceleration";
        break;
    case CriterionId::DERIVATIVE:
        stream << "Derivative";
        break;
    case CriterionId::DIVERGENCE:
        stream << "Divergence";
        break;
    case CriterionId::MAXIMAL_VALUE:
        stream << "Maximal value";
        break;
    case CriterionId::INITIAL_VALUE:
        stream << "Default value";
        break;
    case CriterionId::MAX_CHANGE:
        stream << "Max. change limit";
        break;
    default:
        NOT_IMPLEMENTED;
    }
    return stream;
}


//-----------------------------------------------------------------------------------------------------------
// DerivativeCriterion implementation
//-----------------------------------------------------------------------------------------------------------

/// Helper class storing a minimal value of time step and corresponding statistics.
template <typename T>
struct MinimalStepTls {
    Float minStep = INFTY;
    T value = T(0._f);
    T derivative = T(0._f);
    Size particleIdx = 0;

    MinimalStepTls(const Float UNUSED(power)) {}

    /// Add a time step to the set, given also value, derivative and particle index
    INLINE void add(const Float step, const T v, const T dv, const Size idx) {
        if (step < minStep) {
            minStep = step;
            value = v;
            derivative = dv;
            particleIdx = idx;
        }
    }

    /// Add a time step computed by other TLS
    INLINE void add(const MinimalStepTls& other) {
        this->add(other.minStep, other.value, other.derivative, other.particleIdx);
    }

    INLINE Size isDefined() const {
        return minStep < INFTY;
    }

    /// Return the computed time step
    INLINE Optional<Float> getStep() const {
        return minStep;
    }

    /// Save auxiliary statistics
    INLINE void saveStats(Statistics& stats) const {
        stats.set(StatisticsId::LIMITING_PARTICLE_IDX, int(particleIdx));
        stats.set(StatisticsId::LIMITING_VALUE, Dynamic(value));
        stats.set(StatisticsId::LIMITING_DERIVATIVE, Dynamic(derivative));
    }
};

/// Helper class storing a (generalized) mean. Statistics do not apply here (no single particle is the
/// restrictive one). Same interface as in MinimalStepTls.
template <typename T>
struct MeanStepTls {
    NegativeMean mean;

    MeanStepTls(const Float power)
        : mean(power) {}

    INLINE void add(const Float step, const T UNUSED(v), const T UNUSED(dv), const Float UNUSED(idx)) {
        mean.accumulate(step);
    }

    INLINE void add(const MeanStepTls& other) {
        mean.accumulate(other.mean);
    }

    INLINE Optional<Float> getStep() const {
        if (mean.count() > 0) {
            const Float step = mean.compute();
            ASSERT(isReal(step) || step == INFTY, step);
            return step;
        } else {
            return NOTHING;
        }
    }

    INLINE void saveStats(Statistics& UNUSED(stats)) const {
        // do nothing
    }
};


DerivativeCriterion::DerivativeCriterion(const RunSettings& settings) {
    factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR);
    power = settings.get<Float>(RunSettingsId::TIMESTEPPING_MEAN_POWER);
    ASSERT(power < 0._f); // currently not implemented for non-negative powers
}

TimeStep DerivativeCriterion::compute(IScheduler& scheduler,
    Storage& storage,
    const Float maxStep,
    Statistics& stats) {
    VERBOSE_LOG
    if (power < -1.e3_f) {
        // very high negative power, this is effectively computing minimal timestep
        return this->computeImpl<MinimalStepTls>(scheduler, storage, maxStep, stats);
    } else {
        // generic case, compute a generalized mean of timesteps
        return this->computeImpl<MeanStepTls>(scheduler, storage, maxStep, stats);
    }
}

template <template <typename> class Tls>
TimeStep DerivativeCriterion::computeImpl(IScheduler& scheduler,
    Storage& storage,
    const Float maxStep,
    Statistics& stats) {
    Float totalMinStep = INFTY;
    CriterionId minId = CriterionId::INITIAL_VALUE;

    // for each first-order quantity ...
    iterate<VisitorEnum::FIRST_ORDER>(storage, [&](const QuantityId id, auto& v, auto& dv) {
        ASSERT(v.size() == dv.size());
        using T = typename std::decay_t<decltype(v)>::Type;

        // ... and for each particle ...
        Tls<T> result(power);
        ThreadLocal<Tls<T>> tls(scheduler, power);

        auto functor = [&](const Size i, Tls<T>& tls) {
            const auto absdv = abs(dv[i]);
            const auto absv = abs(v[i]);
            const Float minValue = storage.getMaterialOfParticle(i)->minimal(id);
            ASSERT(minValue > 0._f); // some nonzero minimal value must be set for all quantities

            StaticArray<Float, 6> vs = getComponents(absv);
            StaticArray<Float, 6> dvs = getComponents(absdv);
            ASSERT(vs.size() == dvs.size());

            for (Size j = 0; j < vs.size(); ++j) {
                if (abs(vs[j]) < 2._f * minValue) {
                    continue;
                }
                const Float value = factor * (vs[j] + minValue) / (dvs[j] + EPS);
                ASSERT(isReal(value));
                tls.add(value, v[i], dv[i], i);
            }
        };

        parallelFor(scheduler, tls, 0, v.size(), functor);
        // get min step from thread-local results
        for (Tls<T>& tl : tls) {
            result.add(tl);
        }

        // save statistics
        const Optional<Float> minStep = result.getStep();
        if (minStep && minStep.value() < totalMinStep) {
            totalMinStep = minStep.value();
            minId = CriterionId::DERIVATIVE;
            stats.set(StatisticsId::LIMITING_QUANTITY, id);
            result.saveStats(stats);
        }
    });
// make sure only 2nd order quanity is positions, they are handled by Acceleration criterion
#ifdef SPH_DEBUG
    Size cnt = 0;
    for (auto q : storage.getQuantities()) {
        if (q.quantity.getOrderEnum() == OrderEnum::SECOND) {
            cnt++;
        }
    }
    ASSERT(cnt == 1);
#endif

    if (totalMinStep > maxStep) {
        totalMinStep = maxStep;
        minId = CriterionId::MAXIMAL_VALUE;
    }
    return { totalMinStep, minId };
}

//-----------------------------------------------------------------------------------------------------------
// AccelerationCriterion implementation
//-----------------------------------------------------------------------------------------------------------

AccelerationCriterion::AccelerationCriterion(const RunSettings& settings) {
    factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_DERIVATIVE_FACTOR);
}

TimeStep AccelerationCriterion::compute(IScheduler& scheduler,
    Storage& storage,
    const Float maxStep,
    Statistics& UNUSED(stats)) {
    VERBOSE_LOG
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    struct Tl {
        Float minStep = INFTY;
    };

    auto functor = [&](const Size i, Tl& tl) {
        const Float dvNorm = getSqrLength(dv[i]);
        if (dvNorm > EPS) {
            const Float step = factor * root<4>(sqr(r[i][H]) / dvNorm);
            ASSERT(isReal(step) && step > 0._f && step < INFTY);
            tl.minStep = min(tl.minStep, step);
        }
    };
    Tl result;
    ThreadLocal<Tl> tls(scheduler);
    parallelFor(scheduler, tls, 0, r.size(), functor);
    for (Tl& tl : tls) {
        result.minStep = min(result.minStep, tl.minStep);
    }

    if (result.minStep > maxStep) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    } else {
        return { result.minStep, CriterionId::ACCELERATION };
    }
}

//-----------------------------------------------------------------------------------------------------------
// DivergenceCriterion implementation
//-----------------------------------------------------------------------------------------------------------

DivergenceCriterion::DivergenceCriterion(const RunSettings& settings) {
    factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_DIVERGENCE_FACTOR);
}

TimeStep DivergenceCriterion::compute(IScheduler& scheduler,
    Storage& storage,
    const Float maxStep,
    Statistics& UNUSED(stats)) {
    VERBOSE_LOG
    if (!storage.has(QuantityId::VELOCITY_DIVERGENCE)) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    }
    ArrayView<const Float> divv = storage.getValue<Float>(QuantityId::VELOCITY_DIVERGENCE);
    struct Tl {
        Float minStep = INFTY;
    };

    auto functor = [&](const Size i, Tl& tl) {
        const Float dv = abs(divv[i]);
        if (dv > EPS) {
            const Float step = factor / dv;
            ASSERT(isReal(step) && step > 0._f && step < INFTY);
            tl.minStep = min(tl.minStep, step);
        }
    };
    Tl result;
    ThreadLocal<Tl> tls(scheduler);
    parallelFor(scheduler, tls, 0, divv.size(), functor);
    for (Tl& tl : tls) {
        result.minStep = min(result.minStep, tl.minStep);
    }

    if (result.minStep > maxStep) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    } else {
        return { result.minStep, CriterionId::DIVERGENCE };
    }
}

//-----------------------------------------------------------------------------------------------------------
// CourantCriterion implementation
//-----------------------------------------------------------------------------------------------------------

CourantCriterion::CourantCriterion(const RunSettings& settings) {
    courant = settings.get<Float>(RunSettingsId::TIMESTEPPING_COURANT_NUMBER);
}


TimeStep CourantCriterion::compute(IScheduler& scheduler,
    Storage& storage,
    const Float maxStep,
    Statistics& UNUSED(stats)) {
    VERBOSE_LOG

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
    ArrayView<const Size> neighs;
    if (storage.has(QuantityId::NEIGHBOUR_CNT)) {
        neighs = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    }

    struct Tl {
        Float minStep = INFTY;
    };

    auto functor = [&](const Size i, Tl& tl) {
        if (cs[i] > 0._f) {
            const Float value = courant * r[i][H] / cs[i];
            ASSERT(isReal(value) && value > 0._f && value < INFTY);
            tl.minStep = min(tl.minStep, value);
        }
    };
    Tl result;
    ThreadLocal<Tl> tls(scheduler);
    parallelFor(scheduler, tls, 0, r.size(), functor);
    for (Tl& tl : tls) {
        result.minStep = min(result.minStep, tl.minStep);
    }

    if (result.minStep > maxStep) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    } else {
        return { result.minStep, CriterionId::CFL_CONDITION };
    }
}

//-----------------------------------------------------------------------------------------------------------
// MultiCriterion implementation
//-----------------------------------------------------------------------------------------------------------

MultiCriterion::MultiCriterion(const RunSettings& settings) {
    const Flags<TimeStepCriterionEnum> flags =
        settings.getFlags<TimeStepCriterionEnum>(RunSettingsId::TIMESTEPPING_CRITERION);
    if (flags.has(TimeStepCriterionEnum::COURANT)) {
        criteria.push(makeAuto<CourantCriterion>(settings));
    }
    if (flags.has(TimeStepCriterionEnum::DERIVATIVES)) {
        criteria.push(makeAuto<DerivativeCriterion>(settings));
    }
    if (flags.has(TimeStepCriterionEnum::ACCELERATION)) {
        criteria.push(makeAuto<AccelerationCriterion>(settings));
    }
    if (flags.has(TimeStepCriterionEnum::DIVERGENCE)) {
        criteria.push(makeAuto<DivergenceCriterion>(settings));
    }

    maxChange = settings.get<Float>(RunSettingsId::TIMESTEPPING_MAX_INCREASE);
    lastStep = settings.get<Float>(RunSettingsId::TIMESTEPPING_INITIAL_TIMESTEP);
}

MultiCriterion::MultiCriterion(Array<AutoPtr<ITimeStepCriterion>>&& criteria,
    const Float maxChange,
    const Float initial)
    : criteria(std::move(criteria))
    , maxChange(maxChange)
    , lastStep(initial) {}

TimeStep MultiCriterion::compute(IScheduler& scheduler,
    Storage& storage,
    const Float maxStep,
    Statistics& stats) {
    VERBOSE_LOG
    ASSERT(!criteria.empty());
    Float minStep = INFTY;
    CriterionId minId = CriterionId::INITIAL_VALUE;
    for (auto& crit : criteria) {
        const TimeStep step = crit->compute(scheduler, storage, maxStep, stats);
        if (step.value < minStep) {
            minStep = step.value;
            minId = step.id;
        }
    }

    // smooth the timestep if required
    if (maxChange < INFTY) {
        const Float maxStep = lastStep * (1._f + maxChange);
        if (minStep > maxStep) {
            minStep = maxStep;
            minId = CriterionId::MAX_CHANGE;
        }
        lastStep = minStep;
    }

    // we don't have to limit by maxStep as each criterion is limited separately
    ASSERT(minStep < INFTY);
    return { minStep, minId };
}

NAMESPACE_SPH_END
