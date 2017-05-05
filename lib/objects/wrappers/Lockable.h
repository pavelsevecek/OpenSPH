#pragma once

/// \file Lockable.h
/// \brief Object associated with a mutex
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/AutoPtr.h"
#include <atomic>
#include <mutex>

NAMESPACE_SPH_BEGIN

template <typename T>
class LockedPtr {
    template <typename>
    friend class Lockable;

private:
    T& data;
    std::unique_lock<std::mutex> lock;
    std::atomic<Size>& cnt;

    LockedPtr(T& data, std::mutex& mutex, std::atomic<Size>& cnt)
        : data(data)
        , lock(mutex)
        , cnt(cnt) {
        ++cnt;
    }

public:
    LockedPtr(LockedPtr&& other)
        : data(other.data)
        , lock(std::move(other.lock))
        , cnt(other.cnt) {
        other.cnt = 0;
    }

    ~LockedPtr() {
        if (lock.owns_lock()) {
            ASSERT(cnt > 0);
            --cnt;
        }
    }

    /// \todo test if this is correct to use with && ref-qualifier
    T* operator->() const {
        return &data;
    }

    T& operator*() const & {
        return data;
    }

    T& operator*() const && = delete;


    void release() {
        lock.unlock();
        ASSERT(cnt > 0);
        --cnt;
    }
};

template <typename T>
class Lockable : public Noncopyable {
private:
    struct Resource {
        T data;
        std::mutex mutex;

        Resource() = default;

        Resource(const T& data)
            : data(data) {}

        Resource(T&& data)
            : data(std::move(data)) {}
    };
    AutoPtr<Resource> ptr;

    /// \todo only for debug & assert?
    std::atomic<Size> proxyCnt;

public:
    Lockable()
        : ptr(makeAuto<Resource>()) {
        proxyCnt = 0;
    }

    Lockable(const T& value)
        : ptr(makeAuto<Resource>(value)) {
        proxyCnt = 0;
    }

    Lockable(T&& value)
        : ptr(makeAuto<Resource>(std::move(value))) {
        proxyCnt = 0;
    }

    Lockable(Lockable&& other)
        : ptr(std::move(other.ptr)) {
        proxyCnt = other.proxyCnt;
        other.proxyCnt = 0;
    }

    ~Lockable() {
        ASSERT(proxyCnt == 0);
    }

    Lockable& operator=(Lockable&& other) {
        ptr = std::move(other.ptr);
        proxyCnt = other.proxyCnt.exchange(proxyCnt);
        return *this;
    }

    LockedPtr<T> lock() {
        ASSERT(proxyCnt == 0);
        return LockedPtr<T>(ptr->data, ptr->mutex, proxyCnt);
    }

    LockedPtr<const T> lock() const {
        ASSERT(proxyCnt == 0);
        return LockedPtr<const T>(ptr->data, ptr->mutex, proxyCnt);
    }

    LockedPtr<T> operator->() {
        ASSERT(proxyCnt == 0);
        return LockedPtr<T>(ptr->data, ptr->mutex, proxyCnt);
    }

    LockedPtr<const T> operator->() const {
        ASSERT(proxyCnt == 0);
        return LockedPtr<const T>(ptr->data, ptr->mutex, proxyCnt);
    }
};

NAMESPACE_SPH_END
