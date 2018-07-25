#include "thread/Scheduler.h"
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

NAMESPACE_SPH_END
