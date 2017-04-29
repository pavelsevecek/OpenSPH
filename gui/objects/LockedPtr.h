#pragma once

#include <common/Assert.h>
#include <memory>
#include <mutex>

NAMESPACE_SPH_BEGIN

/// Smart pointer also managing a lock. The lock is locked in constructor and unlocked in destructor.
template <typename Type>
class LockedPtr : public Noncopyable {
private:
    std::shared_ptr<Type> ptr;
    std::unique_lock<std::mutex> lock;

public:
    LockedPtr(const std::shared_ptr<Type>& ptr, std::mutex& mutex)
        : ptr(ptr)
        , lock(mutex) {}

    Type& operator*() {
        ASSERT(ptr);
        return *ptr;
    }

    explicit operator bool() {
        return ptr != nullptr;
    }
};

NAMESPACE_SPH_END
