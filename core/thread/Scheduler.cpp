#include "thread/Scheduler.h"
#include "math/MathUtils.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

Optional<Size> SequentialScheduler::getThreadIdx() const {
    // imitating ThreadPool with 1 thread, so that we can use ThreadLocal with this scheduler
    return 0;
}

Size SequentialScheduler::getThreadCnt() const {
    // imitating ThreadPool with 1 thread, so that we can use ThreadLocal with this scheduler
    return 1;
}

Size SequentialScheduler::getRecommendedGranularity() const {
    return 1;
}

void SequentialScheduler::parallelFor(const Size from,
    const Size to,
    const Size UNUSED(granularity),
    const RangeFunctor& functor) {
    functor(from, to);
}

void SequentialScheduler::parallelInvoke(const Functor& func1, const Functor& func2) {
    func1();
    func2();
}

SharedPtr<SequentialScheduler> SequentialScheduler::getGlobalInstance() {
    static SharedPtr<SequentialScheduler> scheduler = makeShared<SequentialScheduler>();
    return scheduler;
}

SequentialScheduler SEQUENTIAL;

NAMESPACE_SPH_END
