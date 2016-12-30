#pragma once

#include "quantities/Storage.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN

class TimeStep : public Object {
private:
    Float factor;
    Float courant;

public:
    TimeStep(const GlobalSettings& settings) {
        factor = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
        courant = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);
    }

    /// Returns the time step based on ratio between quantities and their derivatives
    Float get(Storage& storage, const Float maxStep) const {
        PROFILE_SCOPE("TimeStep::get");
        Float minStep = INFTY;
        // Find highest step from ratios 'value/derivative'
        iterate<VisitorEnum::FIRST_ORDER>(storage, [this, &minStep](auto&& v, auto&& dv) {
            ASSERT(v.size() == dv.size());
            for (int i = 0; i < v.size(); ++i) {
                if (Math::normSqr(dv[i]) != 0._f) {
                    /// \todo here, float quantities could be significantly optimized using SSE
                    minStep = Math::min(minStep, factor * Math::norm(v[i]) / Math::norm(dv[i]));
                    ASSERT(Math::isReal(minStep) && minStep > 0._f && minStep < INFTY);
                }
            }
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
        for (int i = 0; i < r.size(); ++i) {
            minStep = Math::min(minStep, courant * r[i][H] / cs[i]);
        }

        // Make sure the step is lower than largest allowed step
        minStep = Math::min(minStep, maxStep);
        return minStep;
    }

private:
    template <typename TArray>
    Float cond2ndOrder(TArray&&, TArray&&) const {
        NOT_IMPLEMENTED;
    }

    Float cond2ndOrder(LimitedArray<Vector>& v, LimitedArray<Vector>& d2v) const {
        ASSERT(v.size() == d2v.size());
        Float minStep = INFTY;
        for (int i = 0; i < v.size(); ++i) {
            const Float d2vNorm = Math::normSqr(d2v[i]);
            if (d2vNorm != 0._f) {
                const Float step = Math::root<4>(Math::sqr(v[i][H]) / d2vNorm);
                minStep = Math::min(minStep, step);
                ASSERT(Math::isReal(minStep) && minStep > 0._f && minStep < INFTY);
            }
        }
        return minStep;
    }
};

NAMESPACE_SPH_END
