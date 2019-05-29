#pragma once

/// \file Tbb.h
/// \brief Implements IScheduler interface using TBB.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

struct TbbData;

/// \brief Scheduler encapsulating Intel Thread Building Blocks (TBB).
///
/// TBB libraries need to be linked in order to use this scheduler.
class Tbb : public IScheduler {
private:
    AutoPtr<TbbData> data;

    static SharedPtr<Tbb> globalInstance;

public:
    Tbb(const Size numThreads = 0, const Size granularity = 1000);

    ~Tbb();

    void setGranularity(const Size newGranularity);

    virtual SharedPtr<ITask> submit(const Function<void()>& task) override;

    virtual Optional<Size> getThreadIdx() const override;

    virtual Size getThreadCnt() const override;

    virtual Size getRecommendedGranularity() const override;

    virtual void parallelFor(const Size from,
        const Size to,
        const Size granularity,
        const Function<void(Size n1, Size n2)>& functor) override;

    static SharedPtr<Tbb> getGlobalInstance();
};


NAMESPACE_SPH_END
