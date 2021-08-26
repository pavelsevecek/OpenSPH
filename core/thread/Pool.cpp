#include "thread/Pool.h"
#include "objects/wrappers/Finally.h"

NAMESPACE_SPH_BEGIN

SharedPtr<ThreadPool> ThreadPool::globalInstance;

struct ThreadContext {
    /// Owner of this thread
    ThreadPool* parentPool = nullptr;

    /// Index of this thread in the parent thread pool (not std::this_thread::get_id() !)
    Size index = Size(-1);

    /// Task currently processed by this thread
    SharedPtr<Task> current = nullptr;
};

static thread_local ThreadContext threadLocalContext;

Task::Task(const Function<void()>& callable)
    : callable(callable) {}

Task::~Task() {
    SPH_ASSERT(this->completed());
}

void Task::wait() {
    if (threadLocalContext.parentPool) {
        // worker thread, we can work on tasks
        while (tasksLeft > 0) {
            threadLocalContext.parentPool->processTask(false);
        }
    } else {
        std::unique_lock<std::mutex> lock(waitMutex);
        if (tasksLeft > 0) {
            // non-worker thread, simply wait until no tasks are left
            waitVar.wait(lock, [this] { return tasksLeft == 0; });
        }
    }
    SPH_ASSERT(tasksLeft == 0);

    if (caughtException) {
        std::rethrow_exception(caughtException);
    }
}

bool Task::completed() const {
    return tasksLeft == 0;
}

bool Task::isRoot() const {
    return parent == nullptr;
}

SharedPtr<Task> Task::getParent() const {
    return parent;
}

SharedPtr<Task> Task::getCurrent() {
    return threadLocalContext.current;
}

void Task::setParent(SharedPtr<Task> task) {
    parent = task;

    // sanity check to avoid circular dependency
    SPH_ASSERT(!parent || parent->getParent().get() != RawPtr<Task>(this));

    if (task) {
        task->addReference();
    }
}

void Task::setException(std::exception_ptr exception) {
    if (this->isRoot()) {
        caughtException = exception;
    } else {
        parent->setException(exception);
    }
}

void Task::runAndNotify() {
    // this may be called from within another task, so we override the threadLocalContext.current for
    // this scope only
    SharedPtr<Task> callingTask = threadLocalContext.current;
    threadLocalContext.current = this->sharedFromThis();
    auto guard = finally([this, callingTask] {
        threadLocalContext.current = callingTask;
        this->removeReference();
    });

    try {
        callable();
    } catch (...) {
        // store caught exception, replacing the previous one
        this->setException(std::current_exception());
    }
}

void Task::addReference() {
    std::unique_lock<std::mutex> lock(waitMutex);
    SPH_ASSERT(tasksLeft > 0);
    tasksLeft++;
}

void Task::removeReference() {
    std::unique_lock<std::mutex> lock(waitMutex);
    tasksLeft--;
    SPH_ASSERT(tasksLeft >= 0);

    if (tasksLeft == 0) {
        if (!this->isRoot()) {
            parent->removeReference();
        }
        waitVar.notify_all();
    }
}

ThreadPool::ThreadPool(const Size numThreads, const Size granularity)
    : threads(numThreads == 0 ? std::thread::hardware_concurrency() : numThreads)
    , granularity(granularity) {
    SPH_ASSERT(!threads.empty());
    auto loop = [this](const Size index) {
        // setup the thread
        threadLocalContext.parentPool = this;
        threadLocalContext.index = index;

        // run the loop
        while (!stop) {
            this->processTask(true);
        }
    };
    stop = false;
    tasksLeft = 0;

    // start all threads
    Size index = 0;
    for (auto& t : threads) {
        t = makeAuto<std::thread>(loop, index);
        ++index;
    }
}

ThreadPool::~ThreadPool() {
    waitForAll();
    stop = true;
    taskVar.notify_all();

    for (auto& t : threads) {
        if (t->joinable()) {
            t->join();
        }
    }
}

SharedPtr<ITask> ThreadPool::submit(const Function<void()>& task) {
    SharedPtr<Task> handle = makeShared<Task>(task);
    handle->setParent(threadLocalContext.current);

    {
        std::unique_lock<std::mutex> lock(waitMutex);
        ++tasksLeft;
    }
    {
        std::unique_lock<std::mutex> lock(taskMutex);
        tasks.emplace(handle);
    }
    taskVar.notify_all();
    return handle;
}

void ThreadPool::processTask(const bool wait) {
    SharedPtr<Task> task = this->getNextTask(wait);
    if (task) {
        // run the task
        task->runAndNotify();

        std::unique_lock<std::mutex> lock(waitMutex);
        --tasksLeft;
    } else {
        SPH_ASSERT(!wait || stop);
    }
    waitVar.notify_one();
}

void ThreadPool::waitForAll() {
    std::unique_lock<std::mutex> lock(waitMutex);
    if (tasksLeft > 0) {
        waitVar.wait(lock, [this] { return tasksLeft == 0; });
    }
    SPH_ASSERT(tasks.empty() && tasksLeft == 0);
}

Size ThreadPool::getRecommendedGranularity() const {
    return granularity;
}

Optional<Size> ThreadPool::getThreadIdx() const {
    if (threadLocalContext.parentPool != this || threadLocalContext.index == Size(-1)) {
        // thread either belongs to different ThreadPool or isn't a worker thread
        return NOTHING;
    }
    return threadLocalContext.index;
}

Size ThreadPool::getThreadCnt() const {
    return threads.size();
}

void ThreadPool::parallelFor(const Size from,
    const Size to,
    const Size granularity,
    const RangeFunctor& functor) {
    SPH_ASSERT(to >= from);
    SPH_ASSERT(granularity > 0);

    SharedPtr<Task> handle = this->submit([this, from, to, granularity, &functor] {
        for (Size n = from; n < to; n += granularity) {
            const Size n1 = n;
            const Size n2 = min(n1 + granularity, to);
            this->submit([n1, n2, &functor] { functor(n1, n2); });
        }
    });
    handle->wait();
    SPH_ASSERT(handle->completed());
}

void ThreadPool::parallelInvoke(const Functor& task1, const Functor& task2) {
    SharedPtr<Task> handle = this->submit(task1);
    task2();
    handle->wait();
    SPH_ASSERT(handle->completed());
}

SharedPtr<ThreadPool> ThreadPool::getGlobalInstance() {
    if (!globalInstance) {
        globalInstance = makeShared<ThreadPool>();
    }
    return globalInstance;
}

SharedPtr<Task> ThreadPool::getNextTask(const bool wait) {
    std::unique_lock<std::mutex> lock(taskMutex);

    if (wait) {
        // wait till a task is available
        taskVar.wait(lock, [this] { return !tasks.empty() || stop; });
    }

    // remove the task from the queue and return it
    if (!stop && !tasks.empty()) {
        SharedPtr<Task> task = tasks.front();
        tasks.pop();
        return task;
    } else {
        return nullptr;
    }
}

NAMESPACE_SPH_END
