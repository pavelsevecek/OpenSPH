#include "sph/timestepping/AdaptiveTimeStep.h"

NAMESPACE_SPH_BEGIN

AdaptiveTimeStep::AdaptiveTimeStep(const GlobalSettings& settings) {
    factor = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
    courant = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);
}

Float AdaptiveTimeStep::get(Storage& storage, const Float maxStep) const {
    PROFILE_SCOPE("TimeStep::get");
    Float minStep = INFTY;
    Array<QuantityKey> quantities;
    for (auto& q : storage) {
        if (q.second.getOrderEnum() == OrderEnum::FIRST_ORDER) {
            quantities.push(q.first);
        }
    }
    // Find highest step from ratios 'value/derivative'
    int idx = 0;
    QuantityKey flag = QuantityKey::MATERIAL_IDX;
    iterate<VisitorEnum::FIRST_ORDER>(
        storage, [this, &flag, &quantities, &idx, &minStep](auto&& v, auto&& dv) {
            ASSERT(v.size() == dv.size());
            for (Size i = 0; i < v.size(); ++i) {
                if (Math::normSqr(dv[i]) != 0._f) {
                    /// \todo here, float quantities could be significantly optimized using SSE
                    const Float value = factor * Math::norm(v[i]) / Math::norm(dv[i]);
                    if (value < minStep) {
                        minStep = value;
                        flag = quantities[idx];
                    }
                    ASSERT(Math::isReal(minStep) && minStep > 0._f && minStep < INFTY);
                }
            }
            idx++;
        });
    /// \todo currently hard-coded for positions only
    iterate<VisitorEnum::SECOND_ORDER>(
        storage, [this, &minStep](auto&& v, auto&& UNUSED(dv), auto&& d2v) {
            minStep = Math::min(minStep, this->cond2ndOrder(v, d2v));
        });

    // Courant criterion
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityKey::POSITIONS);
    ArrayView<Float> cs = storage.getValue<Float>(QuantityKey::SOUND_SPEED);
    /// \todo AV contribution?
    for (Size i = 0; i < r.size(); ++i) {
        const Float value = courant * r[i][H] / cs[i];
        if (value < minStep) {
            minStep = value;
            flag = QuantityKey::SOUND_SPEED;
        }
    }

    // Make sure the step is lower than largest allowed step
    minStep = Math::min(minStep, maxStep);
    /// \todo logger
    // std::cout << "Step set by " << getQuantityName(flag) << std::endl;
    return minStep;
}

Float AdaptiveTimeStep::cond2ndOrder(LimitedArray<Vector>& v, LimitedArray<Vector>& d2v) const {
    ASSERT(v.size() == d2v.size());
    Float minStep = INFTY;
    for (Size i = 0; i < v.size(); ++i) {
        const Float d2vNorm = Math::normSqr(d2v[i]);
        if (d2vNorm != 0._f) {
            const Float step = Math::root<4>(Math::sqr(v[i][H]) / d2vNorm);
            minStep = Math::min(minStep, step);
            ASSERT(Math::isReal(minStep) && minStep > 0._f && minStep < INFTY);
        }
    }
    return minStep;
}

NAMESPACE_SPH_END
