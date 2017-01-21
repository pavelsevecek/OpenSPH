#pragma once

/// Concurrent queue
/// Pavel Sevecek 2017
/// sevecek@sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Optional.h"
#include <queue>
#include <mutex>

NAMESPACE_SPH_BEGIN

/// \todo can be implemented as lock-free queue, see boost::lockfree::queue

template <typename Type>
class ConcurrentQueue {
private:
    std::queue<Type> queue;

    std::mutex mutex;

public:
    ConcurrentQueue() = default;

    /// Pushes the element to the back of the queue. This is a thread-safe operation.
    template <typename T>
    void push(T&& value) {
        std::unique_lock<std::mutex> lock(mutex);
        queue.push(std::forward<T>(value));
    }

    /// Removes and returns element from the front of the queue. If the queue is empty, returns NOTHING. This
    /// is a thread-safe operation.
    Optional<Type> pop() {
        std::unique_lock<std::mutex> lock(mutex);
        if (queue.empty()) {
            return NOTHING;
        }
        Type value = queue.front();
        queue.pop();
        return value;
    }

    /// Checks whether the queue is empty. This is a thread-safe operation.
    bool empty() {
        std::unique_lock<std::mutex> lock(mutex);
        return queue.empty();
    }
};

NAMESPACE_SPH_END
