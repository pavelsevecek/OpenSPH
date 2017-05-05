#include "timestepping/TimeStepCriterion.h"
#include "io/Logger.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Iterate.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include "system/Statistics.h"
#include "thread/AtomicFloat.h"

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

DerivativeCriterion::DerivativeCriterion(const RunSettings& settings) {
    factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
}

Tuple<Float, CriterionId> DerivativeCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> stats) {
    Float totalMinStep = INFTY;
    CriterionId minId = CriterionId::INITIAL_VALUE;

    iterate<VisitorEnum::FIRST_ORDER>(storage, [&](const QuantityId id, auto&& v, auto&& dv) {
        ASSERT(v.size() == dv.size());
        using T = typename std::decay_t<decltype(v)>::Type;
        struct Tl {
            Float minStep = INFTY;
            T value = T(0._f);
            T derivative = T(0._f);
            Size particleIdx = 0;

            INLINE void add(const Float step, const T v, const T dv, const Float idx) {
                if (step < minStep) {
                    minStep = step;
                    value = v;
                    derivative = dv;
                    particleIdx = idx;
                }
            }
        };
        auto functor = [&](const Size n1, const Size n2, Tl& tl) {
            for (Size i = n1; i < n2; ++i) {
                const auto absdv = abs(dv[i]);
                const auto absv = abs(v[i]);
                const Float minValue = storage.getMaterialOfParticle(i)->minimal(id);
                ASSERT(minValue > 0._f); // some nonzero minimal value must be set for all quantities
                if (norm(absv) < minValue) {
                    continue;
                }
                using TAbs = decltype(absv);
                const auto value = factor * (absv + TAbs(minValue)) / (absdv + TAbs(EPS));
                ASSERT(isReal(value));
                const Float e = minElement(value);
                tl.add(e, v[i], dv[i], i);
            }
        };
        Tl result;
        if (auto pool = storage.getThreadPool()) {
            ThreadLocal<Tl> tls(*pool);
            parallelFor(*pool, tls, 0, v.size(), 1000, functor);
            // get min step from thread-local results
            tls.forEach([&result](Tl& tl) { //
                result.add(tl.minStep, tl.value, tl.derivative, tl.particleIdx);
            });
        } else {
            functor(0, v.size(), result);
        }
        // save statistics
        if (result.minStep < totalMinStep) {
            totalMinStep = result.minStep;
            minId = CriterionId::DERIVATIVE;
            if (stats) {
                stats->set(StatisticsId::LIMITING_QUANTITY, id);
                stats->set(StatisticsId::LIMITING_PARTICLE_IDX, int(result.particleIdx));
                stats->set(StatisticsId::LIMITING_VALUE, Value(result.value));
                stats->set(StatisticsId::LIMITING_DERIVATIVE, Value(result.derivative));
            }
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


Tuple<Float, CriterionId> AccelerationCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> UNUSED(stats)) {
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    struct Tl {
        Float minStep = INFTY;
    };

    auto functor = [&](const Size n1, const Size n2, Tl& tl) {
        for (Size i = n1; i < n2; ++i) {
            const Float dvNorm = getSqrLength(dv[i]);
            if (dvNorm > EPS) {
                const Float step = root<4>(sqr(r[i][H]) / dvNorm);
                ASSERT(isReal(step) && step > 0._f && step < INFTY);
                tl.minStep = min(tl.minStep, step);
            }
        }
    };
    Tl result;
    if (auto pool = storage.getThreadPool()) {
        ThreadLocal<Tl> tls(*pool);
        parallelFor(*pool, tls, 0, r.size(), 1000, functor);
        tls.forEach([&result](Tl& tl) { result.minStep = min(result.minStep, tl.minStep); });
    } else {
        functor(0, r.size(), result);
    }
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
    Optional<Statistics&> UNUSED(stats)) {

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
    if (auto pool = storage.getThreadPool()) {
        ThreadLocal<Tl> tls(*pool);
        parallelFor(*pool, tls, 0, r.size(), 1000, functor);
        tls.forEach([&result](Tl& tl) { result.minStep = min(result.minStep, tl.minStep); });
    } else {
        functor(0, r.size(), result);
    }
    if (result.minStep > maxStep) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    } else {
        return { result.minStep, CriterionId::CFL_CONDITION };
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MultiCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

MultiCriterion::MultiCriterion(const RunSettings& settings)
    : criteria(EMPTY_ARRAY) {
    const Flags<TimeStepCriterionEnum> flags =
        Flags<TimeStepCriterionEnum>::fromValue(settings.get<int>(RunSettingsId::TIMESTEPPING_CRITERION));
    if (flags.has(TimeStepCriterionEnum::COURANT)) {
        criteria.push(makeAuto<CourantCriterion>(settings));
    }
    if (flags.has(TimeStepCriterionEnum::DERIVATIVES)) {
        criteria.push(makeAuto<DerivativeCriterion>(settings));
    }
    if (flags.has(TimeStepCriterionEnum::ACCELERATION)) {
        criteria.push(makeAuto<AccelerationCriterion>());
    }
}

Tuple<Float, CriterionId> MultiCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> stats) {
    PROFILE_SCOPE("MultiCriterion::compute");
    ASSERT(!criteria.empty());
    Float minStep = INFTY;
    CriterionId minId = CriterionId::INITIAL_VALUE;
    for (auto& crit : criteria) {
        Float step;
        CriterionId id;
        /// \todo proper copying of optional reference
        Optional<Statistics&> statsClone;
        if (stats) {
            statsClone.emplace(stats.value());
        }
        tieToTuple(step, id) = crit->compute(storage, maxStep, std::move(statsClone));
        if (step < minStep) {
            minStep = step;
            minId = id;
        }
    }
    // we don't have to limit by maxStep as each criterion is limited separately
    ASSERT(minStep < INFTY);
    return { minStep, minId };
}

NAMESPACE_SPH_END
