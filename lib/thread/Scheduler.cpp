#include "thread/Scheduler.h"
#include "math/Math.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

class SequentialTaskHandle : public ITask {
public:
    virtual void wait() override {
        // when we return this handle, the task has been already finished, so we do not have to wait
    }

    virtual bool completed() const override {
        // when we return this handle, the task has been already finished
        return true;
    }
};

SharedPtr<ITask> SequentialScheduler::submit(const Function<void()>& task) {
    task();
    return makeShared<SequentialTaskHandle>();
}

Optional<Size> SequentialScheduler::getThreadIdx() const {
    // imitating ThreadPool with 1 thread, so that we can use ThreadLocal with this scheduler
    return 0;
}

Size SequentialScheduler::getThreadCnt() const {
    // imitating ThreadPool with 1 thread, so that we can use ThreadLocal with this scheduler
    return 1;
}

Size SequentialScheduler::getRecommendedGranularity(const Size from, const Size to) const {
    // just in case someone actually uses this in parallelFor, let's avoid the splitting by returning maximal
    // granularity
    return to - from;
}

SharedPtr<SequentialScheduler> SequentialScheduler::getGlobalInstance() {
    static SharedPtr<SequentialScheduler> scheduler = makeShared<SequentialScheduler>();
    return scheduler;
}

SequentialScheduler SEQUENTIAL;


void IScheduler::parallelFor(const Size from,
    const Size to,
    const Size granularity,
    const Function<void(Size n1, Size n2)>& functor) {
    ASSERT(to > from);
    ASSERT(granularity > 0);

    SharedPtr<ITask> handle = this->submit([this, from, to, granularity, &functor] {
        for (Size n = from; n < to; n += granularity) {
            const Size n1 = n;
            const Size n2 = min(n1 + granularity, to);
            this->submit([n1, n2, &functor] { functor(n1, n2); });
        }
    });
    handle->wait();
    ASSERT(handle->completed());
}

NAMESPACE_SPH_END
