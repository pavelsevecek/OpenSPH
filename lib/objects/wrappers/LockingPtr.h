#pragma once

/// \file LockingPtr.h
/// \brief Smart pointer associated with a mutex
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/AlignedStorage.h"
#include "objects/wrappers/SharedPtr.h"
#include <atomic>
#include <mutex>

NAMESPACE_SPH_BEGIN

namespace Detail {
    template <typename T>
    class LockingControlBlock : public ControlBlock<T> {
    private:
        std::mutex mutex;
        bool locked = false;

    public:
        LockingControlBlock(T* ptr)
            : ControlBlock<T>(ptr) {}

        void lock() {
            mutex.lock();
            locked = true;
        }

        void unlock() {
            mutex.unlock();
            locked = false;
        }

        bool isLocked() const {
            return locked;
        }
    };
}

template <typename T>
class LockingPtr {
    template <typename>
    friend class LockingPtr;

private:
    SharedPtr<T> resource;
    Detail::LockingControlBlock<T>* block = nullptr;

#ifdef SPH_DEBUG
    mutable std::atomic_flag proxyFlag = ATOMIC_FLAG_INIT;
#else
    // add dummy variable to that LockingPtr has the same size in Debug and Release
    AlignedStorage<std::atomic_flag> padding;
#endif

public:
    LockingPtr() = default;

    LockingPtr(T* ptr)
        : resource(ptr, ptr ? new Detail::LockingControlBlock<T>(ptr) : nullptr) {
        block = static_cast<Detail::LockingControlBlock<T>*>(resource.block);
    }

    LockingPtr(const LockingPtr& other)
        : resource(other.resource)
        , block(other.block) {}

    template <typename T2>
    LockingPtr(const LockingPtr<T2>& other)
        : resource(other.resource)
        , block(other.block) {}

    LockingPtr(LockingPtr&& other)
        : resource(std::move(other.resource))
        , block(other.block) {
        other.block = nullptr;
    }

    template <typename T2>
    LockingPtr(LockingPtr<T2>&& other)
        : resource(std::move(other.resource))
        , block((Detail::LockingControlBlock<T>*)other.block) {
        // type safety is already guarandeed by SharedPtr, so hopefully we can simply cast the block (the
        // Locking block only adds a mutex that's independend on T)
        other.block = nullptr;
    }

    ~LockingPtr() {
        if (block) {
            // make sure all the proxies are destroyed before killing the object
            std::unique_lock<Detail::LockingControlBlock<T>> lock(*block);
        }
    }

    LockingPtr& operator=(const LockingPtr& other) {
        if (block) {
            std::unique_lock<Detail::LockingControlBlock<T>> lock(*block);
            resource = other.resource;
            block = other.block;
        } else {
            resource = other.resource;
            block = other.block;
        }
        return *this;
    }

    LockingPtr& operator=(LockingPtr&& other) {
        if (block) {
            std::unique_lock<Detail::LockingControlBlock<T>> lock(*block);
            resource = std::move(other.resource);
            std::swap(block, other.block);
        } else {
            resource = std::move(other.resource);
            std::swap(block, other.block);
        }
        return *this;
    }

    class Proxy {
        template <typename>
        friend class LockingPtr;

    private:
        RawPtr<T> ptr;

#ifdef SPH_DEBUG
        // must be initialized before the lock is locked
        std::atomic_flag* flag = nullptr;
#endif
        std::unique_lock<Detail::LockingControlBlock<T>> lock;

        Proxy()
            : ptr(nullptr) {}

#ifdef SPH_DEBUG
        Proxy(const SharedPtr<T>& ptr, Detail::LockingControlBlock<T>* block, std::atomic_flag& flag)
            : ptr(ptr.get())
            , flag(this->initFlag(flag))
            , lock(*block) {
            ASSERT(ptr != nullptr);
        }
#else
        Proxy(const SharedPtr<T>& ptr, Detail::LockingControlBlock<T>* block)
            : ptr(ptr.get())
            , lock(*block) {
            ASSERT(ptr != nullptr);
        }
#endif

    public:
        Proxy(Proxy&& proxy)
            : ptr(proxy.ptr)
#ifdef SPH_DEBUG
            , flag(proxy.flag)
#endif
            , lock(std::move(proxy.lock)) {
        }

#ifdef SPH_DEBUG
        std::atomic_flag* initFlag(std::atomic_flag& flag) {
            ASSERT(!flag.test_and_set(), "LockingPtr can only be locked once within a scope");
            return std::addressof(flag);
        }

        ~Proxy() {
            if (flag) {
                flag->clear();
            }
        }
#endif

        RawPtr<T> operator->() {
            ASSERT(ptr != nullptr);
            return ptr;
        }

        T& operator*() {
            ASSERT(ptr != nullptr);
            return *ptr;
        }

        RawPtr<T> get() {
            return ptr;
        }

        bool isLocked() {
            return lock.owns_lock();
        }

        void release() {
            if (lock.owns_lock()) {
                lock.unlock();
            }
            ptr = nullptr;
        }
    };

    struct ProxyRef {
        Proxy proxy;

        operator T&() {
            return *proxy;
        }
    };

    Proxy lock() const {
        if (resource) {
#ifdef SPH_DEBUG
            return Proxy(resource, block, proxyFlag);
#else
            return Proxy(resource, block);
#endif
        } else {
            return Proxy();
        }
    }

    Proxy operator->() const {
        ASSERT(resource);
#ifdef SPH_DEBUG
        return Proxy(resource, block, proxyFlag);
#else
        return Proxy(resource, block);
#endif
    }

    ProxyRef operator*() const {
        ASSERT(resource);
#ifdef SPH_DEBUG
        return ProxyRef{ Proxy(resource, block, proxyFlag) };
#else
        return ProxyRef{ Proxy(resource, block) };
#endif
    }

    explicit operator bool() const {
        return resource;
    }

    bool operator!() const {
        return !resource;
    }

    void reset() {
        if (block) {
            std::unique_lock<Detail::LockingControlBlock<T>> lock(*block);
            block = nullptr;
            resource.reset();
        } else {
            resource.reset();
        }
    }
};

template <typename T, typename... TArgs>
LockingPtr<T> makeLocking(TArgs&&... args) {
    return LockingPtr<T>(new T(std::forward<TArgs>(args)...));
}

NAMESPACE_SPH_END
