#include "objects/finders/NeighborFinder.h"
#include "thread/Scheduler.h"

NAMESPACE_SPH_BEGIN

void IBasicFinder::build(IScheduler& scheduler, ArrayView<const Vector> points) {
    values = points;
    this->buildImpl(scheduler, values);
}

static Order makeRankH(ArrayView<const Vector> values, Flags<FinderFlag> flags) {
    if (flags.has(FinderFlag::MAKE_RANK)) {
        return makeRank(values.size(), [values](const Size i1, const Size i2) { //
            return values[i1][H] < values[i2][H];
        });
    } else {
        return Order();
    }
}

void ISymmetricFinder::build(IScheduler& scheduler, ArrayView<const Vector> points, Flags<FinderFlag> flags) {
    values = points;
    rank = makeRankH(values, flags);
    this->buildImpl(scheduler, values);
}

NAMESPACE_SPH_END
