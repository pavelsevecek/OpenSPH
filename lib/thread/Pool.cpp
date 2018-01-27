#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

ThreadPool* ThreadPool::globalInstance;

struct ThreadContext {
    /// Owner of this thread
    const ThreadPool* parentPool = nullptr;

    /// Index of this thread in the parent thread pool (not std::this_thread::get_id() !)
    Size index = Size(-1);
};

static thread_local ThreadContext threadLocalContext;

ThreadPool::ThreadPool(const Size numThreads)
    : threads(numThreads == 0 ? std::thread::hardware_concurrency() : numThreads) {
    ASSERT(!threads.empty());
    auto loop = [this](const Size index) {
        // setup the thread
        threadLocalContext.parentPool = this;
        threadLocalContext.index = index;

        // run the loop
        while (!stop) {
            AutoPtr<ITask> task = this->getNextTask();
            if (task) {
                // run the task
                try {
                    (*task)();
                } catch (...) {
                    // store caught exception, replacing the previous one
                    //   std::unique_lock<std::mutex> lock(exceptionMutex);
                    caughtException = std::current_exception();
                }

                std::unique_lock<std::mutex> lock(waitMutex);
                --tasksLeft;
            }
            waitVar.notify_one();
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

void ThreadPool::submit(AutoPtr<ITask>&& task) {
    {
        std::unique_lock<std::mutex> lock(waitMutex);
        ++tasksLeft;
    }
    {
        std::unique_lock<std::mutex> lock(taskMutex);
        tasks.emplace(std::move(task));
    }
    taskVar.notify_one();
}

void ThreadPool::waitForAll() {
    std::unique_lock<std::mutex> lock(waitMutex);
    if (tasksLeft > 0) {
        waitVar.wait(lock, [this] { return tasksLeft == 0; });
    }
    ASSERT(tasks.empty() && tasksLeft == 0);
    /// \todo find better way to rethrow the exception?
    if (caughtException) {
        std::exception_ptr current = caughtException;
        caughtException = nullptr; // null to avoid throwing it twice
        std::rethrow_exception(current);
    }
}

Optional<Size> ThreadPool::getThreadIdx() const {
    if (threadLocalContext.parentPool != this || threadLocalContext.index == Size(-1)) {
        // thread either belongs to different ThreadPool or isn't a worker thread
        return NOTHING;
    }
    return threadLocalContext.index;
}

ThreadPool& ThreadPool::getGlobalInstance() {
    if (!globalInstance) {
        globalInstance = new ThreadPool();
    }
    return *globalInstance;
}

AutoPtr<ITask> ThreadPool::getNextTask() {
    std::unique_lock<std::mutex> lock(taskMutex);

    // wait till a task is available
    taskVar.wait(lock, [this] { return tasks.size() || stop; });
    // execute task
    if (!stop && !tasks.empty()) {
        AutoPtr<ITask> task = std::move(tasks.front());
        tasks.pop();
        return task;
    } else {
        return nullptr;
    }
}

NAMESPACE_SPH_END
