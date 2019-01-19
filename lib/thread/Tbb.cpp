#include "thread/Tbb.h"
#include "math/MathUtils.h"
#include "objects/wrappers/Optional.h"

#ifdef SPH_USE_TBB
#include <tbb/tbb.h>
#endif

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_TBB

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
        ASSERT(!tbbThreadContext.task, "waiting on child tasks is currently not implemented");
        arena.execute([this] { group.wait(); });
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

    TbbData(const Size numThreads) {
        arena.initialize(numThreads == 0 ? tbb::task_scheduler_init::default_num_threads() : numThreads);
    }
};

Tbb::Tbb(const Size numThreads) {
    data = makeAuto<TbbData>(numThreads);
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

void Tbb::parallelFor(const Size from,
    const Size to,
    const Size granularity,
    const Function<void(Size, Size)>& functor) {
    data->arena.execute([from, to, granularity, &functor] {
        tbb::parallel_for(tbb::blocked_range<Size>(from, to, granularity),
            [&functor](const tbb::blocked_range<Size> range) { functor(range.begin(), range.end()); });
    });
}

SharedPtr<Tbb> Tbb::getGlobalInstance() {
    if (!globalInstance) {
        globalInstance = makeShared<Tbb>();
    }
    return globalInstance;
}

#endif

NAMESPACE_SPH_END
