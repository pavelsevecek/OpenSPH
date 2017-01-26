#include "sph/timestepping/AdaptiveTimeStep.h"
#include "quantities/Iterate.h"

NAMESPACE_SPH_BEGIN

AdaptiveTimeStep::AdaptiveTimeStep(const GlobalSettings& settings) {
    factor = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
    courant = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);
}

Float AdaptiveTimeStep::get(Storage& storage, const Float maxStep, Statistics& stats) {
    PROFILE_SCOPE("TimeStep::get");
    cachedSteps.reserve(storage.getParticleCnt());
    StaticArray<Float, 16> minTimeSteps(EMPTY_ARRAY);

    // Find the step from ratios 'value/derivative'
    iterate<VisitorEnum::FIRST_ORDER>(storage, [&](const QuantityIds id, auto&& v, auto&& dv) {
        ASSERT(v.size() == dv.size());
        this->cachedSteps.clear();
        Quantity& quantity = storage.getQuantity(id);
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
            const Float minStep = minElement(value);
            this->cachedSteps.push(minStep);
        }
        const Float totalMinStep = this->cachedSteps.empty() ? INFTY : minOfArray(this->cachedSteps);
        minTimeSteps.push(totalMinStep);
    });

    // Find the step from second-order equantities
    /// \todo currently hard-coded for positions only
    Float minStepAcceleration = INFTY;
    iterate<VisitorEnum::SECOND_ORDER>(storage,
        [this, &minStepAcceleration](
            const QuantityIds UNUSED_IN_RELEASE(id), auto&& v, auto&& UNUSED(dv), auto&& d2v) {
            ASSERT(id == QuantityIds::POSITIONS);
            minStepAcceleration = this->cond2ndOrder(v, d2v);
        });

    // Courant criterion
    /// \todo AV contribution?
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityIds::POSITIONS);
    ArrayView<Float> cs = storage.getValue<Float>(QuantityIds::SOUND_SPEED);
    cachedSteps.clear();
    for (Size i = 0; i < r.size(); ++i) {
        const Float value = courant * r[i][H] / cs[i];
        ASSERT(isReal(value) && value > 0._f && value < INFTY);
        cachedSteps.push(value);
    }
    Float minStepCourant = cachedSteps.empty() ? INFTY : minOfArray(cachedSteps);
    // Find the lowest step and set flag in stats
    StaticArray<QuantityIds, 16> quantities(EMPTY_ARRAY);
    for (auto& q : storage) {
        if (q.second.getOrderEnum() == OrderEnum::FIRST_ORDER) {
            quantities.push(q.first);
        }
    }
    minTimeSteps.push(minStepAcceleration);
    quantities.push(QuantityIds::POSITIONS); // placeholder for acceleration
    minTimeSteps.push(minStepCourant);
    quantities.push(QuantityIds::SOUND_SPEED); // placeholder for Courant criterion

    QuantityIds flag = QuantityIds::MATERIAL_IDX; // dummy quantity
    Float minStep = INFTY;
    ASSERT(minTimeSteps.size() == quantities.size());
    for (Size i = 0; i < minTimeSteps.size(); ++i) {
        if (minTimeSteps[i] < minStep) {
            minStep = minTimeSteps[i];
            flag = quantities[i];
        }
    }

    // Make sure the step is lower than largest allowed step
    if (minStep > maxStep) {
        minStep = maxStep;
        flag = QuantityIds::MAXIMUM_VALUE;
    }

    stats.set(StatisticsIds::TIMESTEP_VALUE, minStep);
    stats.set(StatisticsIds::TIMESTEP_CRITERION, flag);

    return minStep;
}

Float minOfArray(Array<Float>& ar) {
    ASSERT(!ar.empty());
    for (Size step = 2; step < 2 * ar.size(); step *= 2) {
        for (Size i = 0; i < ar.size() - (step >> 1); i += step) {
            ar[i] = min(ar[i], ar[i + (step >> 1)]);
        }
    }
    return ar[0];
}

Float AdaptiveTimeStep::cond2ndOrder(Array<Vector>& v, Array<Vector>& d2v) {
    ASSERT(v.size() == d2v.size());
    cachedSteps.clear();
    for (Size i = 0; i < v.size(); ++i) {
        const Float d2vNorm = normSqr(d2v[i]);
        if (d2vNorm > EPS) {
            const Float step = root<4>(sqr(v[i][H]) / d2vNorm);
            ASSERT(isReal(step) && step > 0._f && step < INFTY);
            cachedSteps.push(step);
        }
    }
    return cachedSteps.empty() ? INFTY : minOfArray(cachedSteps);
}

NAMESPACE_SPH_END
