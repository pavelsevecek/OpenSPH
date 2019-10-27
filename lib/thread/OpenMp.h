#pragma once

#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

/// \brief Scheduler encapsulating OpenMP directives.
///
/// Need to be compiled with -fopenmp.
class OmpScheduler : public IScheduler {
private:
    static SharedPtr<OmpScheduler> globalInstance;

public:
    OmpScheduler(const Size numThreads = 0);

    virtual SharedPtr<ITask> submit(const Function<void()>& task) override;

    virtual Optional<Size> getThreadIdx() const override;

    virtual Size getThreadCnt() const override;

    virtual Size getRecommendedGranularity() const override;

    virtual void parallelFor(const Size from,
        const Size to,
        const Size granularity,
        const Function<void(Size n1, Size n2)>& functor) override;

    static SharedPtr<OmpScheduler> getGlobalInstance();
};


NAMESPACE_SPH_END
