#include "timestepping/TimeStepCriterion.h"
#include "io/Logger.h"
#include "quantities/AbstractMaterial.h"
#include "quantities/Iterate.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DerivativeCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

DerivativeCriterion::DerivativeCriterion(const RunSettings& settings) {
    factor = settings.get<Float>(RunSettingsId::TIMESTEPPING_ADAPTIVE_FACTOR);
}

Tuple<Float, CriterionId> DerivativeCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> stats) {
    PROFILE_SCOPE("DerivativeCriterion::compute");
    Float totalMinStep = INFTY;
    CriterionId minId = CriterionId::INITIAL_VALUE;

    iterate<VisitorEnum::FIRST_ORDER>(storage, [&](const QuantityId id, auto&& v, auto&& dv) {
        ASSERT(v.size() == dv.size());
        Float minStep = INFTY;
        using T = typename std::decay_t<decltype(v)>::Type;
        struct {
            T value = T(0._f);
            T derivative = T(0._f);
            Size particleIdx = 0;
        } limit;
        ArrayView<Size> matIdxs = storage.getValue<Size>(QuantityId::MATERIAL_IDX);
        for (Size i = 0; i < v.size(); ++i) {
            const auto absdv = abs(dv[i]);
            const auto absv = abs(v[i]);
            const Float minValue = storage.getMaterial(matIdxs[i])->minimal(id);
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
            minId = CriterionId::DERIVATIVE;
            if (stats) {
                stats->set(StatisticsId::LIMITING_QUANTITY, id);
                stats->set(StatisticsId::LIMITING_PARTICLE_IDX, int(limit.particleIdx));
                stats->set(StatisticsId::LIMITING_VALUE, Value(limit.value));
                stats->set(StatisticsId::LIMITING_DERIVATIVE, Value(limit.derivative));
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
    PROFILE_SCOPE("AccelerationCriterion::compute");
    Float totalMinStep = INFTY;
    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        const Float dvNorm = getSqrLength(dv[i]);
        if (dvNorm > EPS) {
            const Float step = root<4>(sqr(r[i][H]) / dvNorm);
            ASSERT(isReal(step) && step > 0._f && step < INFTY);
            totalMinStep = min(totalMinStep, step);
        }
    }
    if (totalMinStep > maxStep) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    } else {
        return { totalMinStep, CriterionId::ACCELERATION };
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
    PROFILE_SCOPE("CourantCriterion::compute");
    Float totalMinStep = INFTY;

    /// \todo AV contribution?
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> cs = storage.getValue<Float>(QuantityId::SOUND_SPEED);
    for (Size i = 0; i < r.size(); ++i) {
        if (cs[i] > 0._f) {
            const Float value = courant * r[i][H] / cs[i];
            ASSERT(isReal(value) && value > 0._f && value < INFTY);
            totalMinStep = min(totalMinStep, value);
        }
    }
    if (totalMinStep > maxStep) {
        return { maxStep, CriterionId::MAXIMAL_VALUE };
    } else {
        return { totalMinStep, CriterionId::CFL_CONDITION };
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// MultiCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

MultiCriterion::MultiCriterion(const RunSettings& settings)
    : criteria(EMPTY_ARRAY) {
    const Flags<TimeStepCriterionEnum> flags{ TimeStepCriterionEnum(
        settings.get<int>(RunSettingsId::TIMESTEPPING_CRITERION)) };
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

Tuple<Float, CriterionId> MultiCriterion::compute(Storage& storage,
    const Float maxStep,
    Optional<Statistics&> stats) {
    ASSERT(!criteria.empty());
    Float minStep = INFTY;
    CriterionId minId = CriterionId::INITIAL_VALUE;
    for (auto& crit : criteria) {
        Float step;
        CriterionId id;
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
