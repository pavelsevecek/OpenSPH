#pragma once

#include "storage/Storage.h"
#include "system/Profiler.h"
#include <memory>

NAMESPACE_SPH_BEGIN

class TimeStepGetter : public Object {
private:
    Float factor;
    std::shared_ptr<Storage> storage;

public:
    TimeStepGetter(const std::shared_ptr<Storage>& storage)
        : storage(storage) {}

    /// Returns the time step based on ratio between quantities and their derivatives
    virtual Float operator()(const Float maxStep) const {
        PROFILE_SCOPE("StepGetter::operator()");
        Float minStep = INFTY;
        iterate<TemporalEnum::FIRST_ORDER>(*storage, [this, &minStep](auto&& v, auto&& dv) {
            ASSERT(v.size() == dv.size());
            for (int i = 0; i < v.size(); ++i) {
                if (Math::normSqr(dv[i]) != 0._f) {
                    /// \todo here, float quantities could be significantly optimized using SSE
                    minStep = Math::min(minStep, factor * Math::norm(v[i]) / Math::norm(dv[i]));
                }
            }
        });
        ASSERT(Math::isReal(minStep) && minStep > 0._f && minStep < INFTY);
        return Math::min(minStep, maxStep);
    }
};

NAMESPACE_SPH_END
