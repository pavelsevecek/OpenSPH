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
                task.value()();
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
}

Optional<Size> ThreadPool::getThreadIdx() const {
    std::thread::id id = std::this_thread::get_id();
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
