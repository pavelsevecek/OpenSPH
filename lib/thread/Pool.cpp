#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

ThreadPool* ThreadPool::globalInstance;

ThreadPool::ThreadPool(const Size numThreads)
    : threads(numThreads == 0 ? std::thread::hardware_concurrency() : numThreads) {
    ASSERT(!threads.empty());
    auto loop = [this] {
        while (!stop) {
            Optional<Task> task = getNextTask();
            if (task) {
                // run the task
                try {
                    task.value()();
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
    for (auto& t : threads) {
        t = makeAuto<std::thread>(loop);
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
    std::thread::id id = std::this_thread::get_id();
    /// \todo optimize by storing the ids in static thread_local struct
    for (Size i = 0; i < threads.size(); ++i) {
        if (threads[i]->get_id() == id) {
            return i;
        }
    }
    return NOTHING;
}

ThreadPool& ThreadPool::getGlobalInstance() {
    if (!globalInstance) {
        globalInstance = new ThreadPool();
    }
    return *globalInstance;
}

Optional<ThreadPool::Task> ThreadPool::getNextTask() {
    std::unique_lock<std::mutex> lock(taskMutex);

    // wait till a task is available
    taskVar.wait(lock, [this] { return tasks.size() || stop; });
    // execute task
    if (!stop && !tasks.empty()) {
        Task task = tasks.front();
        tasks.pop();
        return task;
    } else {
        return NOTHING;
    }
}

NAMESPACE_SPH_END
