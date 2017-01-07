#include "sph/timestepping/AdaptiveTimeStep.h"

NAMESPACE_SPH_BEGIN

AdaptiveTimeStep::AdaptiveTimeStep(const GlobalSettings& settings) {
    factor = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
    courant = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);
}

Float AdaptiveTimeStep::get(Storage& storage, const Float maxStep) {
    PROFILE_SCOPE("TimeStep::get");
    cachedSteps.reserve(storage.getParticleCnt());
    StaticArray<Float, 16> minTimeSteps(EMPTY_ARRAY);

    // Find the step from ratios 'value/derivative'
    iterate<VisitorEnum::FIRST_ORDER>(storage, [this, &minTimeSteps](auto&& v, auto&& dv) {
        ASSERT(v.size() == dv.size());
        this->cachedSteps.clear();
        for (Size i = 0; i < v.size(); ++i) {
            const Float dvSqrNorm = Math::normSqr(dv[i]);
            if (dvSqrNorm != 0._f) {
                /// \todo here, float quantities could be significantly optimized using SSE
                const Float value = factor * Math::norm(v[i]) / Math::sqrtApprox(dvSqrNorm);
                ASSERT(Math::isReal(value) && value > 0._f && value < INFTY);
                this->cachedSteps.push(value);
            }
        }
        const Float minStep= this->cachedSteps.empty() ? INFTY : minOfArray(this->cachedSteps);
        minTimeSteps.push(minStep);
    });

    // Find the step from second-order equantities
    /// \todo currently hard-coded for positions only
    Float minStepAcceleration = INFTY;
    iterate<VisitorEnum::SECOND_ORDER>(
        storage, [this, &minStepAcceleration](auto&& v, auto&& UNUSED(dv), auto&& d2v) {
            minStepAcceleration = this->cond2ndOrder(v, d2v);
        });

    // Courant criterion
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    ArrayView<Float> cs = storage.getValue<Float>(QuantityKey::SOUND_SPEED);
    /// \todo AV contribution?

    cachedSteps.clear();
    for (Size i = 0; i < r.size(); ++i) {
        const Float value = courant * r[i][H] / cs[i];
        ASSERT(Math::isReal(value) && value > 0._f && value < INFTY);
        cachedSteps.push(value);
    }
    Float minStepCourant = cachedSteps.empty() ? INFTY : minOfArray(cachedSteps);
    // Find the lowest step and set flag in stats
    StaticArray<QuantityKey, 16> quantities(EMPTY_ARRAY);
    for (auto& q : storage) {
        if (q.second.getOrderEnum() == OrderEnum::FIRST_ORDER) {
            quantities.push(q.first);
        }
    }
    minTimeSteps.push(minStepAcceleration);
    quantities.push(QuantityKey::POSITIONS); // placeholder for acceleration
    minTimeSteps.push(minStepCourant);
    quantities.push(QuantityKey::SOUND_SPEED); // placeholder for Courant criterion

    QuantityKey flag = QuantityKey::MATERIAL_IDX; // dummy quantity
    Float minStep = INFTY;
    ASSERT(minTimeSteps.size() == quantities.size());
    for (Size i = 0; i < minTimeSteps.size(); ++i) {
        if (minTimeSteps[i] < minStep) {
            minStep = minTimeSteps[i];
            flag = quantities[i];
        }
    }

    // Make sure the step is lower than largest allowed step
    minStep = Math::min(minStep, maxStep);

    return minStep;
}

Float minOfArray(Array<Float>& ar) {
    ASSERT(!ar.empty());
    for (Size step = 2; step < 2 * ar.size(); step *= 2) {
        for (Size i = 0; i < ar.size() - (step >> 1); i += step) {
            ar[i] = Math::min(ar[i], ar[i + (step >> 1)]);
        }
    }
    return ar[0];
}

Float AdaptiveTimeStep::cond2ndOrder(LimitedArray<Vector>& v, LimitedArray<Vector>& d2v) {
    ASSERT(v.size() == d2v.size());
    Float minStep = INFTY;
    cachedSteps.clear();
    for (Size i = 0; i < v.size(); ++i) {
        const Float d2vNorm = Math::normSqr(d2v[i]);
        if (d2vNorm != 0._f) {
            const Float step = Math::root<4>(Math::sqr(v[i][H]) / d2vNorm);
            ASSERT(Math::isReal(minStep) && minStep > 0._f && minStep < INFTY);
            cachedSteps.push(step);
        }
    }
    return cachedSteps.empty() ? INFTY : minOfArray(cachedSteps);
}

NAMESPACE_SPH_END
