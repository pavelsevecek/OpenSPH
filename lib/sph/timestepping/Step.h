#pragma once

#include "storage/Storage.h"
#include "system/Profiler.h"
#include <memory>

NAMESPACE_SPH_BEGIN

class StepGetter : public Object {
private:
    Float factor;
    std::shared_ptr<Storage> storage;

public:
    StepGetter(const std::shared_ptr<Storage>& storage)
        : storage(storage) {}

    /// Returns the time step based on ratio between quantities and their derivatives
    virtual Float operator()() const {
        PROFILE_SCOPE("StepGetter::operator()");
        Float minStep = INFTY;
        iterate<TemporalEnum::FIRST_ORDER>(*storage, [&minStep](auto&& v, auto&& dv) {
            ASSERT(v.size() == dv.size());
            for (int i = 0; i < v.size(); ++i) {
                if (normSqr(dv[i]) != 0._f) {
                    /// \todo here, float quantities could be significantly optimized using SSE
                    minStep = min(minStep, factor * norm(v[i]) / norm(dv[i]));
                }
            }
        });
        ASSERT(minStep > 0._f && minStep < INFTY);
        return minStep;
    }
};

NAMESPACE_SPH_END
