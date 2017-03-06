#include "sph/timestepping/TimeStepCriterion.h"
#include "quantities/Iterate.h"
#include "quantities/AbstractMaterial.h"
#include "system/Logger.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DerivativeCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

DerivativeCriterion::DerivativeCriterion(const GlobalSettings& settings) {
    factor = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
}

Tuple<Float, AllCriterionIds> DerivativeCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> stats) {
    PROFILE_SCOPE("DerivativeCriterion::compute");
    Float totalMinStep = INFTY;
    AllCriterionIds minId = CriterionIds::INITIAL_VALUE;

    iterate<VisitorEnum::FIRST_ORDER>(storage, [&](const QuantityIds id, auto&& v, auto&& dv) {
        ASSERT(v.size() == dv.size());
        Float minStep = INFTY;
        using T = typename std::decay_t<decltype(v)>::Type;
        struct {
            T value = T(0._f);
            T derivative = T(0._f);
            Size particleIdx = 0;
        } limit;

        for (Size i = 0; i < v.size(); ++i) {
            const auto absdv = abs(dv[i]);
            const auto absv = abs(v[i]);
            const Float minValue = MaterialAccessor(storage).minimal(id, i);
            ASSERT(minValue > 0._f); // some nonzero minimal value must be set for all quantities
            if (norm(absv) < minValue) {
                continue;
            }
            using TAbs = decltype(absv);
            const auto value = factor * (absv + TAbs(minValue)) / (absdv + TAbs(EPS));
            ASSERT(isReal(value));
            const Float e = minElement(value);
            if (e < minStep) {
                minStep = e;
                limit = { v[i], dv[i], i };
            }
        }
        if (minStep < totalMinStep) {
            totalMinStep = minStep;
            minId = id;
            if (stats) {
                stats->set(StatisticsIds::LIMITING_QUANTITY, id);
                stats->set(StatisticsIds::LIMITING_PARTICLE_IDX, int(limit.particleIdx));
                stats->set(StatisticsIds::LIMITING_VALUE, Value(limit.value));
                stats->set(StatisticsIds::LIMITING_DERIVATIVE, Value(limit.derivative));
            }
        }
    });
    // make sure only 2nd order quanity is positions, they are handled by Acceleration criterion
    ASSERT(std::count_if(storage.begin(), storage.end(), [](auto&& pair) {
        return pair.second.getOrderEnum() == OrderEnum::SECOND_ORDER;
    }) == 1);

    if (totalMinStep > maxStep) {
        totalMinStep = maxStep;
        minId = CriterionIds::MAXIMAL_VALUE;
    }
    return { totalMinStep, minId };
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// AccelerationCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


Tuple<Float, AllCriterionIds> AccelerationCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> UNUSED(stats)) {
    PROFILE_SCOPE("AccelerationCriterion::compute");
    Float totalMinStep = INFTY;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        const Float dvNorm = getSqrLength(dv[i]);
        if (dvNorm > EPS) {
            const Float step = root<4>(sqr(r[i][H]) / dvNorm);
            ASSERT(isReal(step) && step > 0._f && step < INFTY);
            totalMinStep = min(totalMinStep, step);
        }
    }
    if (totalMinStep > maxStep) {
        return { maxStep, CriterionIds::MAXIMAL_VALUE };
    } else {
        return { totalMinStep, CriterionIds::ACCELERATION };
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CourantCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


CourantCriterion::CourantCriterion(const GlobalSettings& settings) {
    courant = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);
}


Tuple<Float, AllCriterionIds> CourantCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> UNUSED(stats)) {
    PROFILE_SCOPE("CourantCriterion::compute");
    Float totalMinStep = INFTY;

    /// \todo AV contribution?
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    ArrayView<const Float> cs = storage.getValue<Float>(QuantityIds::SOUND_SPEED);
    for (Size i = 0; i < r.size(); ++i) {
        if (cs[i] > 0._f) {
            const Float value = courant * r[i][H] / cs[i];
            ASSERT(isReal(value) && value > 0._f && value < INFTY);
            totalMinStep = min(totalMinStep, value);
        }
    }
    if (totalMinStep > maxStep) {
        return { maxStep, CriterionIds::MAXIMAL_VALUE };
    } else {
        return { totalMinStep, CriterionIds::CFL_CONDITION };
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MultiCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

MultiCriterion::MultiCriterion(const GlobalSettings& settings)
    : criteria(EMPTY_ARRAY) {
    const Flags<TimeStepCriterionEnum> flags{ TimeStepCriterionEnum(
        settings.get<int>(GlobalSettingsIds::TIMESTEPPING_CRITERION)) };
    if (flags.has(TimeStepCriterionEnum::COURANT)) {
        criteria.push(std::make_unique<CourantCriterion>(settings));
    }
    if (flags.has(TimeStepCriterionEnum::DERIVATIVES)) {
        criteria.push(std::make_unique<DerivativeCriterion>(settings));
    }
    if (flags.has(TimeStepCriterionEnum::ACCELERATION)) {
        criteria.push(std::make_unique<AccelerationCriterion>());
    }
}

Tuple<Float, AllCriterionIds> MultiCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> stats) {
    ASSERT(!criteria.empty());
    Float minStep = INFTY;
    AllCriterionIds minId = CriterionIds::INITIAL_VALUE;
    for (auto& crit : criteria) {
        Float step;
        QuantityIds id;
        /// \todo proper copying of optional reference
        Optional<Statistics&> statsClone;
        if (stats) {
            statsClone.emplace(stats.get());
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
