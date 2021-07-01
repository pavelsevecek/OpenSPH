#pragma once

#include "objects/finders/NeighbourFinder.h"
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

    mutable ThreadLocal<Array<NeighbourRecord>> extra;

public:
    PeriodicFinder(AutoPtr<ISymmetricFinder>&& actual, const Box& domain, SharedPtr<IScheduler> scheduler);

    virtual Size findAll(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override;

    virtual Size findAll(const Vector& pos,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override;

    virtual Size findLowerRank(const Size, const Float, Array<NeighbourRecord>&) const override {
        NOT_IMPLEMENTED;
    }

protected:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override {
        actual->build(scheduler, points);
    }
};

NAMESPACE_SPH_END
