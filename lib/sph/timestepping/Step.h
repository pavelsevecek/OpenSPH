#pragma once

#include "storage/Storage.h"
#include "system/Profiler.h"
#include "system/Settings.h"
#include <memory>

NAMESPACE_SPH_BEGIN

class TimeStepGetter : public Object {
private:
    std::shared_ptr<Storage> storage;
    ArrayView<Vector> r;
    ArrayView<Float> cs;
    Float factor;
    Float courant;

public:
    TimeStepGetter(const std::shared_ptr<Storage>& storage, const GlobalSettings& settings)
        : storage(storage) {
        factor = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE_FACTOR);
        courant = settings.get<Float>(GlobalSettingsIds::TIMESTEPPING_COURANT);
    }


    /// Returns the time step based on ratio between quantities and their derivatives
    virtual Float operator()(const Float maxStep) const {
        PROFILE_SCOPE("StepGetter::operator()");
        Float minStep = INFTY;
        // Find highest step from ratios 'value/derivative'
        iterate<VisitorEnum::FIRST_ORDER>(*storage, [this, &minStep](auto&& v, auto&& dv) {
            ASSERT(v.size() == dv.size());
            for (int i = 0; i < v.size(); ++i) {
                if (Math::normSqr(dv[i]) != 0._f) {
                    /// \todo here, float quantities could be significantly optimized using SSE
                    minStep = Math::min(minStep, factor * Math::norm(v[i]) / Math::norm(dv[i]));
                    ASSERT(Math::isReal(minStep) && minStep > 0._f && minStep < INFTY);
                }
            }
        });
        // Courant criterion
        /// \todo AV contribution?
        for (int i = 0; i < r.size(); ++i) {
            minStep = Math::min(minStep, courant * r[i][H] / cs[i]);
        }

        // Make sure the step is lower than largest allowed step
        minStep = Math::min(minStep, maxStep);
        return minStep;
    }
};

NAMESPACE_SPH_END
