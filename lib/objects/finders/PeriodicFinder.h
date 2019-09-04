#pragma once

#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Box.h"
#include "objects/wrappers/AutoPtr.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

// currently fixed to x-direction
class PeriodicFinder : public ISymmetricFinder {
private:
    AutoPtr<ISymmetricFinder> actual;
    Box domain;
    SharedPtr<IScheduler> scheduler;

    mutable ThreadLocal<Array<NeighbourRecord>> extra;

public:
    PeriodicFinder(AutoPtr<ISymmetricFinder>&& actual, const Box& domain, SharedPtr<IScheduler> scheduler)
        : actual(std::move(actual))
        , domain(domain)
        , scheduler(scheduler)
        , extra(*scheduler) {}

    virtual Size findAll(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override {
        return this->findAll(values[index], radius, neighbours);
    }

    virtual Size findAll(const Vector& pos,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override {
        Size count = actual->findAll(pos, radius, neighbours);

        if (pos[X] < domain.lower()[X] + radius) {
            count += actual->findAll(pos + Vector(domain.size()[X], 0._f, 0._f), radius, extra.local());
            neighbours.pushAll(extra.local());
        } else if (pos[X] > domain.upper()[X] - radius) {
            count += actual->findAll(pos - Vector(domain.size()[X], 0._f, 0._f), radius, extra.local());
            neighbours.pushAll(extra.local());
        }
        return count;
    }

    virtual Size findLowerRank(const Size, const Float, Array<NeighbourRecord>&) const override {
        NOT_IMPLEMENTED;
    }

protected:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override {
        actual->build(scheduler, points);
    }
};

NAMESPACE_SPH_END
