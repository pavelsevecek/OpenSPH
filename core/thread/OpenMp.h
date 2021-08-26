#pragma once

#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

/// \brief Scheduler encapsulating OpenMP directives.
///
/// Need to be compiled with -fopenmp.
class OmpScheduler : public IScheduler {
private:
    static SharedPtr<OmpScheduler> globalInstance;

    Size granularity = 100;

public:
    OmpScheduler(const Size numThreads = 0);

    void setGranularity(const Size newGranularity) {
        granularity = newGranularity;
    }

    virtual SharedPtr<ITask> submit(const Function<void()>& task) override;

    virtual Optional<Size> getThreadIdx() const override;

    virtual Size getThreadCnt() const override;

    virtual Size getRecommendedGranularity() const override;

    virtual void parallelFor(const Size from,
        const Size to,
        const Size granularity,
        const RangeFunctor& functor) override;

    virtual void parallelInvoke(const Functor& task1, const Functor& task2) override;

    static SharedPtr<OmpScheduler> getGlobalInstance();
};


NAMESPACE_SPH_END
