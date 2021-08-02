#pragma once

#include "objects/finders/NeighborFinder.h"
#include "objects/geometry/Box.h"
#include "objects/wrappers/AutoPtr.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

/// \brief Finder wrapper respecting periodic domain.
class PeriodicFinder : public ISymmetricFinder {
private:
    AutoPtr<ISymmetricFinder> actual;
    Box domain;
    SharedPtr<IScheduler> scheduler;

    mutable ThreadLocal<Array<NeighborRecord>> extra;

public:
    PeriodicFinder(AutoPtr<ISymmetricFinder>&& actual, const Box& domain, SharedPtr<IScheduler> scheduler);

    virtual Size findAll(const Size index,
        const Float radius,
        Array<NeighborRecord>& neighbors) const override;

    virtual Size findAll(const Vector& pos,
        const Float radius,
        Array<NeighborRecord>& neighbors) const override;

    virtual Size findLowerRank(const Size, const Float, Array<NeighborRecord>&) const override {
        NOT_IMPLEMENTED;
    }

protected:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override {
        actual->build(scheduler, points);
    }
};

NAMESPACE_SPH_END
