#include "objects/finders/PeriodicFinder.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN

PeriodicFinder::PeriodicFinder(AutoPtr<ISymmetricFinder>&& actual,
    const Box& domain,
    SharedPtr<IScheduler> scheduler)
    : actual(std::move(actual))
    , domain(domain)
    , scheduler(scheduler)
    , extra(*scheduler) {}

Size PeriodicFinder::findAll(const Size index, const Float radius, Array<NeighbourRecord>& neighbours) const {
    return this->findAll(values[index], radius, neighbours);
}

static const StaticArray<Vector, 3> UNIT = {
    Vector(1._f, 0._f, 0._f),
    Vector(0._f, 1._f, 0._f),
    Vector(0._f, 0._f, 1._f),
};

Size PeriodicFinder::findAll(const Vector& pos,
    const Float radius,
    Array<NeighbourRecord>& neighbours) const {
    Size count = actual->findAll(pos, radius, neighbours);

    for (Size i = 0; i < 3; ++i) {
        if (pos[i] < domain.lower()[i] + radius) {
            count += actual->findAll(pos + domain.size()[i] * UNIT[i], radius, extra.local());
            neighbours.pushAll(extra.local());
        }
        if (pos[i] > domain.upper()[i] - radius) {
            count += actual->findAll(pos - domain.size()[i] * UNIT[i], radius, extra.local());
            neighbours.pushAll(extra.local());
        }
    }
    return count;
}

NAMESPACE_SPH_END
