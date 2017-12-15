#pragma once

#include "common/Assert.h"
#include <mutex>

NAMESPACE_SPH_BEGIN

/// \brief Wraps given object together with a mutex, locking it every time the object is accessed.
///
/// \todo
template <typename T, typename TMutex = std::mutex>
class Locking {
private:
    T value;
    TMutex mutex;

public:
    Locking() = default;

    Locking(const T& value)
        : value(value) {}

    Locking(T&& value)
        : value(std::move(value)) {}

    ~Locking() {
        // make sure all the proxies are destroyed before killing the object
        std::unique_lock<TMutex> lock(mutex);
    }

    class Proxy {
        template <typename>
        friend class Locking;

    private:
        T& value;
        std::unique_lock<TMutex> lock;

        // private constructor, to avoid manually constructing proxies
        Proxy(T& value, TMutex& mutex)
            : value(value)
            , lock(mutex) {}

    public:
        Proxy(Proxy&& other)
            : value(other.value)
            , lock(std::move(other.lock)) {}

        T* operator->() const {
            return &value
        }

        bool isLocked() {
            return lock.owns_lock();
        }

        void release() {
            if (lock.owns_lock()) {
                lock.unlock();
            }
        }
    };

    Proxy lock() const {
        return Proxy(value, mutex);
    }

    Proxy operator->() const {
        return this->lock();
    }
};

NAMESPACE_SPH_END
