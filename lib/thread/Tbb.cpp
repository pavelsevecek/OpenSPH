#include "thread/Tbb.h"
#include "math/Math.h"
#include "objects/wrappers/Optional.h"
#include <tbb/tbb.h>

NAMESPACE_SPH_BEGIN

SharedPtr<Tbb> Tbb::globalInstance = nullptr;

class TbbTask;

struct TbbThreadContext {
    SharedPtr<TbbTask> task;
};

static thread_local TbbThreadContext tbbThreadContext;

class TbbTask : public ITask, public ShareFromThis<TbbTask> {
private:
    tbb::task_arena& arena;
    tbb::task_group group;
    std::atomic<int> taskCnt;

public:
    explicit TbbTask(tbb::task_arena& arena)
        : arena(arena) {
        // set task to 1 before submitting the root task, to avoid the task being completed
        taskCnt = 1;
    }

    ~TbbTask() noexcept { // needs noexcept because of tbb::task_group member
        ASSERT(this->completed());
    }

    virtual void wait() override {
        group.wait();
    }

    virtual bool completed() const override {
        return taskCnt == 0;
    }

    void submit(const Function<void()>& task) {
        arena.execute([this, task] {
            group.run([this, task] {
                tbbThreadContext.task = this->sharedFromThis();
                task();
                tbbThreadContext.task = nullptr;

                taskCnt--;
            });
        });
    }

    void submitChild(const Function<void()>& task) {
        ASSERT(!this->completed()); // cannot add children to already finished task
        taskCnt++;
        this->submit(task);
    }
};

struct TbbData {
    tbb::task_arena arena;

    TbbData() {
        arena.initialize(tbb::task_scheduler_init::default_num_threads(), 0);
    }
};

Tbb::Tbb() {
    data = makeAuto<TbbData>();
}

Tbb::~Tbb() = default;

SharedPtr<ITask> Tbb::submit(const Function<void()>& task) {
    /// \todo we need to hold the SharedPtr somewhere, otherwise the task_group would be destroyed
    if (tbbThreadContext.task) {
        tbbThreadContext.task->submitChild(task);
        /// \todo this works differently than ThreadPool! Refactor if it's ever necessary to wait for child
        /// tasks!!
        return tbbThreadContext.task;
    } else {
        SharedPtr<TbbTask> handle = makeShared<TbbTask>(data->arena);
        handle->submit(task);
        return handle;
    }
}

Optional<Size> Tbb::getThreadIdx() const {
    const int index = tbb::this_task_arena::current_thread_index();
    if (index >= 0) {
        return index;
    } else {
        return NOTHING;
    }
}

Size Tbb::getThreadCnt() const {
    return tbb::this_task_arena::max_concurrency();
}

Size Tbb::getRecommendedGranularity(const Size from, const Size to) const {
    ASSERT(to > from);
    const int threadCnt = data->arena.max_concurrency();
    return min<Size>(1000, max<Size>((to - from) / threadCnt, 1));
}

SharedPtr<Tbb> Tbb::getGlobalInstance() {
    if (!globalInstance) {
        globalInstance = makeShared<Tbb>();
    }
    return globalInstance;
}

NAMESPACE_SPH_END
