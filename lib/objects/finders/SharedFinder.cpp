#include "objects/finders/SharedFinder.h"

NAMESPACE_SPH_BEGIN

template <typename TBase>
TBase& SharedFinder<TBase>::getInstance() {
    if (!instance) {
        instance = makeAuto<TBase>();
    }
    return instance;
}

template <typename TBase>
Size SharedFinder<TBase>::findAll(const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours) const {
    return instance->findAll(index, radius, neighbours);
}

template <typename TBase>
Size SharedFinder<TBase>::findAll(const Vector& pos,
    const Float radius,
    Array<NeighbourRecord>& neighbours) const {
    return instance->findAll(pos, radius, neighbours);
}

template <typename TBase>
Size SharedFinder<TBase>::findLowerRank(const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours) const {
    return instance->findLowerRank(index, radius, neighbours);
}

template <typename TBase>
void SharedFinder<TBase>::buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) {
    instance->buildImpl(scheduler, points);
}


NAMESPACE_SPH_END
