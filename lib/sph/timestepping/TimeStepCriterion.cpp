#include "sph/timestepping/TimeStepCriterion.h"
#include "quantities/Iterate.h"
#include "system/Profiler.h"
#include "system/Settings.h"

NAMESPACE_SPH_BEGIN


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// DerivativeCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

DerivativeCriterion::DerivativeCriterion(const GlobalSettings& settings) {
    factor = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
}

Tuple<Float, QuantityIds> DerivativeCriterion::compute(Storage& storage, const Float maxStep) {
    PROFILE_SCOPE("DerivativeCriterion::compute");
    Float totalMinStep = INFTY;
    QuantityIds minId = QuantityIds::MATERIAL_IDX;

    iterate<VisitorEnum::FIRST_ORDER>(storage, [&](const QuantityIds id, auto&& v, auto&& dv) {
        ASSERT(v.size() == dv.size());
        Quantity& quantity = storage.getQuantity(id);
        Float minStep = INFTY;
        for (Size i = 0; i < v.size(); ++i) {
            const auto absdv = abs(dv[i]);
            const auto absv = abs(v[i]);
            using T = decltype(absv);
            const Float minValue = quantity.getMinimalValue();
            ASSERT(minValue > 0._f); // some nonzero minimal value must be set for all quantities
            if (norm(absv) < minValue) {
                continue;
            }
            const auto value = factor * (absv + T(minValue)) / (absdv + T(EPS));
            ASSERT(isReal(value));
            minStep = min(minStep, minElement(value));
        }
        if (minStep < totalMinStep) {
            totalMinStep = minStep;
            minId = id;
        }
    });
    // make sure only 2nd order quanity is positions, they are handled by Acceleration criterion
    ASSERT(std::count_if(storage.begin(), storage.end(), [](auto&& pair) {
        return pair.second.getOrderEnum() == OrderEnum::SECOND_ORDER;
    }) == 1);

    if (totalMinStep > maxStep) {
        totalMinStep = maxStep;
        minId = QuantityIds::MAXIMUM_VALUE;
    }
    return { totalMinStep, minId };
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// AccelerationCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


Tuple<Float, QuantityIds> AccelerationCriterion::compute(Storage& storage, const Float maxStep) {
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
        return { maxStep, QuantityIds::MAXIMUM_VALUE };
    } else {
        return { totalMinStep, QuantityIds::POSITIONS };
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// CourantCriterion implementation
/////////////////////////////////////////////////////////////////////////////////////////////////////////////


CourantCriterion::CourantCriterion(const GlobalSettings& settings) {
    courant = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);
}


Tuple<Float, QuantityIds> CourantCriterion::compute(Storage& storage, const Float maxStep) {
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
        return { maxStep, QuantityIds::MAXIMUM_VALUE };
    } else {
        return { totalMinStep, QuantityIds::SOUND_SPEED };
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

Tuple<Float, QuantityIds> MultiCriterion::compute(Storage& storage, const Float maxStep) {
    ASSERT(!criteria.empty());
    Float minStep = INFTY;
    QuantityIds minId = QuantityIds::MATERIAL_IDX;
    for (auto& crit : criteria) {
        Float step;
        QuantityIds id;
        tieToTuple(step, id) = crit->compute(storage, maxStep);
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
