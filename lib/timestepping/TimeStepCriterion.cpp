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
    case CriterionId::MAXIMAL_VALUE:
        stream << "Maximal value";
        break;
    case CriterionId::INITIAL_VALUE:
        stream << "Default value";
        break;
    default:
        NOT_IMPLEMENTED;
    }
    return stream;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DerivativeCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Helper class storing a minimal value of time step and corresponding statistics.
template <typename T>
struct MinimalStepTls {
    Float minStep = INFTY;
    T value = T(0._f);
    T derivative = T(0._f);
    Size particleIdx = 0;

    MinimalStepTls(const Float UNUSED(power)) {}

    /// Add a time step to the set, given also value, derivative and particle index
    INLINE void add(const Float step, const T v, const T dv, const Float idx) {
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

    /// Return the computed time step
    INLINE Float getStep() const {
        return minStep;
    }

    /// Save auxiliary statistics
    INLINE void saveStats(Statistics& stats) const {
        stats.set(StatisticsId::LIMITING_PARTICLE_IDX, int(particleIdx));
        stats.set(StatisticsId::LIMITING_VALUE, Value(value));
        stats.set(StatisticsId::LIMITING_DERIVATIVE, Value(derivative));
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

    INLINE Float getStep() const {
        return mean.compute();
    }

    INLINE void saveStats(Statistics& UNUSED(stats)) const {
        // do nothing
    }
};


DerivativeCriterion::DerivativeCriterion(const RunSettings& settings) {
    factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
    power = settings.get<Float>(RunSettingsId::TIMESTEPPING_MEAN_POWER);
    ASSERT(power < 0._f); // currently not implemented for non-negative powers
}

Tuple<Float, CriterionId> DerivativeCriterion::compute(Storage& storage,
    const Float maxStep,
    Statistics& stats) {
    if (power < -1.e3_f) {
        // very high negative power, this is effectively computing minimal timestep
        return this->computeImpl<MinimalStepTls>(storage, maxStep, stats);
    } else {
        // generic case, compute a generalized mean of timesteps
        return this->computeImpl<MeanStepTls>(storage, maxStep, stats);
    }
}

template <template <typename> class Tls>
Tuple<Float, CriterionId> DerivativeCriterion::computeImpl(Storage& storage,
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
        ThreadPool& pool = ThreadPool::getGlobalInstance();
        ThreadLocal<Tls<T>> tls(pool, power);

        auto functor = [&](const Size n1, const Size n2, Tls<T>& tls) {
            for (Size i = n1; i < n2; ++i) {
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
            }
        };

        parallelFor(pool, tls, 0, v.size(), 100, functor);
        // get min step from thread-local results
        tls.forEach([&result](Tls<T>& tl) { //
            result.add(tl);
        });

        // save statistics
        const Float minStep = result.getStep();
        if (minStep < totalMinStep) {
            totalMinStep = minStep;
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


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// AccelerationCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


AccelerationCriterion::AccelerationCriterion(const RunSettings& settings) {
    factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
}

Tuple<Float, CriterionId> AccelerationCriterion::compute(Storage& storage,
    const Float maxStep,
    Statistics& UNUSED(stats)) {
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    struct Tl {
        Float minStep = INFTY;
    };

    auto functor = [&](const Size n1, const Size n2, Tl& tl) {
        for (Size i = n1; i < n2; ++i) {
            const Float dvNorm = getSqrLength(dv[i]);
            if (dvNorm > EPS) {
                const Float step = factor * root<4>(sqr(r[i][H]) / dvNorm);
                ASSERT(isReal(step) && step > 0._f && step < INFTY);
                tl.minStep = min(tl.minStep, step);
            }
        }
    };
    Tl result;
    ThreadPool& pool = ThreadPool::getGlobalInstance();
    ThreadLocal<Tl> tls(pool);
    parallelFor(pool, tls, 0, r.size(), 1000, functor);
    tls.forEach([&result](Tl& tl) { result.minStep = min(result.minStep, tl.minStep); });

    if (result.minStep > maxStep) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    } else {
        return { result.minStep, CriterionId::ACCELERATION };
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CourantCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


CourantCriterion::CourantCriterion(const RunSettings& settings) {
    courant = settings.get<Float>(RunSettingsId::TIMESTEPPING_COURANT);
}


Tuple<Float, CriterionId> CourantCriterion::compute(Storage& storage,
    const Float maxStep,
    Statistics& UNUSED(stats)) {

    /// \todo AV contribution?
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
    struct Tl {
        Float minStep = INFTY;
    };

    auto functor = [&](const Size n1, const Size n2, Tl& tl) {
        for (Size i = n1; i < n2; ++i) {
            if (cs[i] > 0._f) {
                const Float value = courant * r[i][H] / cs[i];
                ASSERT(isReal(value) && value > 0._f && value < INFTY);
                tl.minStep = min(tl.minStep, value);
            }
        }
    };
    Tl result;
    ThreadPool& pool = ThreadPool::getGlobalInstance();
    ThreadLocal<Tl> tls(pool);
    parallelFor(pool, tls, 0, r.size(), 1000, functor);
    tls.forEach([&result](Tl& tl) { result.minStep = min(result.minStep, tl.minStep); });

    if (result.minStep > maxStep) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    } else {
        return { result.minStep, CriterionId::CFL_CONDITION };
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MultiCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

    maxChange = settings.get<Float>(RunSettingsId::TIMESTEPPING_MAX_CHANGE);
}

Tuple<Float, CriterionId> MultiCriterion::compute(Storage& storage, const Float maxStep, Statistics& stats) {
    PROFILE_SCOPE("MultiCriterion::compute");
    ASSERT(!criteria.empty());
    Float minStep = INFTY;
    CriterionId minId = CriterionId::INITIAL_VALUE;
    for (auto& crit : criteria) {
        Float step;
        CriterionId id;
        tieToTuple(step, id) = crit->compute(storage, maxStep, stats);
        if (step < minStep) {
            minStep = step;
            minId = id;
        }
    }

    // smooth the timestep if required
    if (maxChange < INFTY && lastStep > 0._f) {
        minStep = clamp(minStep, lastStep * (1._f - maxChange), lastStep * (1._f + maxChange));
    }
    lastStep = minStep;

    // we don't have to limit by maxStep as each criterion is limited separately
    ASSERT(minStep < INFTY);
    return { minStep, minId };
}

NAMESPACE_SPH_END
