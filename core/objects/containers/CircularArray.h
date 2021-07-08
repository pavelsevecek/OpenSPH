#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

template <typename T>
class CircularArray {
private:
    Array<T> queue;
    Size head = 0;

public:
    CircularArray(const Size maxSize)
        : queue(0, maxSize) {}

    void push(const T& value) {
        if (queue.size() < queue.capacity()) {
            queue.push(value);
        } else {
            queue[head] = value;
            head = this->wrap(1);
        }
    }

    void push(T&& value) {
        if (queue.size() < queue.capacity()) {
            queue.push(std::move(value));
        } else {
            queue[head] = std::move(value);
            head = this->wrap(1);
        }
    }

    Size size() const {
        return queue.size();
    }

    INLINE const T& operator[](const Size i) const {
        SPH_ASSERT(i < queue.size());
        return queue[this->wrap(i)];
    }

    INLINE T& operator[](const Size i) {
        SPH_ASSERT(i < queue.size());
        return queue[this->wrap(i)];
    }

private:
    Size wrap(const Size i) const {
        return (i + head) % queue.size();
    }
};

NAMESPACE_SPH_END
