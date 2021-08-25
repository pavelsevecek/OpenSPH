#pragma once

/// \file Tbb.h
/// \brief Implements IScheduler interface using TBB.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/BasicAllocators.h"
#include "objects/wrappers/PropagateConst.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

struct TbbData;

/// \brief Scheduler encapsulating Intel Thread Building Blocks (TBB).
///
/// TBB libraries need to be linked in order to use this scheduler.
class Tbb : public IScheduler {
private:
    PropagateConst<AutoPtr<TbbData>> data;

    static SharedPtr<Tbb> globalInstance;

public:
    Tbb(const Size numThreads = 0, const Size granularity = 1000);

    ~Tbb();

    void setGranularity(const Size newGranularity);

    void setThreadCnt(const Size numThreads);

    virtual Optional<Size> getThreadIdx() const override;

    virtual Size getThreadCnt() const override;

    virtual Size getRecommendedGranularity() const override;

    virtual void parallelFor(const Size from,
        const Size to,
        const Size granularity,
        const RangeFunctor& functor) override;

    virtual void parallelInvoke(const Functor& task1, const Functor& task2) override;

    static SharedPtr<Tbb> getGlobalInstance();
};

struct TbbAllocator {
    MemoryBlock allocate(const std::size_t size, const std::size_t align) noexcept;
    void deallocate(MemoryBlock& block) noexcept;
};

NAMESPACE_SPH_END
