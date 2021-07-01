#pragma once

/// \file AutoPtr.h
/// \brief Simplified implementation of std::unique_ptr, using only default deleter.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/wrappers/AutoPtr.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

namespace Detail {
class ControlBlockHolder : public Polymorphic {
private:
    std::atomic<int> useCnt;
    std::atomic<int> weakCnt;

public:
    INLINE ControlBlockHolder() {
        useCnt = 1;
        weakCnt = 1;
    }

    INLINE int increaseUseCnt() {
        const int cnt = ++useCnt;
        SPH_ASSERT(cnt > 0);
        return cnt;
    }

    INLINE int getUseCount() const {
        return useCnt;
    }

    INLINE int increaseWeakCnt() {
        const int cnt = ++weakCnt;
        SPH_ASSERT(cnt > 0);
        return cnt;
    }

    INLINE bool increaseUseCntIfNonzero() {
        while (true) {
            int cnt = useCnt;
            if (cnt == 0 || useCnt.compare_exchange_strong(cnt, cnt + 1)) {
                return cnt != 0;
            }
        }
    }

    INLINE void decreaseUseCnt() {
        const int cnt = --useCnt;
        SPH_ASSERT(cnt >= 0);
        if (cnt == 0) {
            this->deletePtr();
        }
    }

    INLINE void decreaseWeakCnt() {
        const int cnt = --weakCnt;
        SPH_ASSERT(cnt >= 0);
        if (cnt == 0) {
            this->deleteBlock();
        }
    }

    virtual void* getPtr() = 0;

    virtual void deletePtr() = 0;

    INLINE void deleteBlock() {
        alignedDelete(this);
    }
};

template <typename T>
class ControlBlock : public ControlBlockHolder {
private:
    T* ptr;

public:
    ControlBlock(T* ptr)
        : ptr(ptr) {}

    INLINE virtual void* getPtr() override {
        SPH_ASSERT(ptr);
        return ptr;
    }

    virtual void deletePtr() override {
        alignedDelete(ptr);
    }
};
} // namespace Detail

template <typename T>
class SharedPtr {
    template <typename>
    friend class SharedPtr;
    template <typename>
    friend class WeakPtr;
    template <typename>
    friend class LockingPtr;
    template <typename>
    friend class SharedFromThis;

protected:
    T* ptr;
    Detail::ControlBlockHolder* block;

    explicit SharedPtr(T* ptr, Detail::ControlBlockHolder* block)
        : ptr(ptr)
        , block(block) {}

public:
    SharedPtr()
        : ptr(nullptr)
        , block(nullptr) {}

    explicit SharedPtr(T* ptr)
        : ptr(ptr) {
        if (ptr) {
            block = alignedNew<Detail::ControlBlock<T>>(ptr);
        } else {
            block = nullptr;
        }
        setSharedFromThis(*this);
    }

    SharedPtr(const SharedPtr& other)
        : ptr(other.ptr) {
        this->copyBlock(other);
    }

    template <typename T2>
    SharedPtr(const SharedPtr<T2>& other)
        : ptr(other.ptr) {
        this->copyBlock(other);
    }

    SharedPtr(SharedPtr&& other)
        : ptr(other.ptr)
        , block(other.block) {
        other.ptr = nullptr;
        other.block = nullptr;
    }

    template <typename T2>
    SharedPtr(SharedPtr<T2>&& other)
        : ptr(other.ptr)
        , block(other.block) {
        other.ptr = nullptr;
        other.block = nullptr;
    }

    template <typename T2>
    SharedPtr(AutoPtr<T2>&& ptr)
        : SharedPtr(ptr.release()) {}

    SharedPtr(std::nullptr_t)
        : ptr(nullptr)
        , block(nullptr) {}

    SharedPtr& operator=(const SharedPtr& other) {
        this->reset();
        ptr = other.ptr;
        this->copyBlock(other);
        return *this;
    }

    template <typename T2>
    SharedPtr& operator=(const SharedPtr<T2>& other) {
        this->reset();
        ptr = other.ptr;
        this->copyBlock(other);
        return *this;
    }

    SharedPtr& operator=(SharedPtr&& other) {
        std::swap(ptr, other.ptr);
        std::swap(block, other.block);
        return *this;
    }

    template <typename T2>
    SharedPtr& operator=(SharedPtr<T2>&& other) {
        this->reset();
        ptr = other.ptr;
        block = other.block;
        other.ptr = nullptr;
        other.block = nullptr;
        return *this;
    }

    SharedPtr& operator=(std::nullptr_t) {
        this->reset();
        return *this;
    }

    ~SharedPtr() {
        this->reset();
    }

    INLINE T* operator->() const {
        SPH_ASSERT(ptr);
        return ptr;
    }

    INLINE T& operator*() const {
        SPH_ASSERT(ptr);
        return *ptr;
    }

    INLINE explicit operator bool() const {
        return ptr != nullptr;
    }

    INLINE bool operator!() const {
        return ptr == nullptr;
    }

    INLINE RawPtr<T> get() const {
        return ptr;
    }

    INLINE void reset() {
        if (block) {
            block->decreaseUseCnt();
            block->decreaseWeakCnt();
            block = nullptr;
        }
        ptr = nullptr;
    }

    INLINE T* release() {
        if (block) {
            block->deleteBlock();
            block = nullptr;
            return ptr;
        } else {
            return nullptr;
        }
    }

    INLINE Size getUseCount() {
        if (!block) {
            return 0;
        } else {
            return block->getUseCount();
        }
    }

    template <typename... TArgs>
    INLINE decltype(auto) operator()(TArgs&&... args) const {
        SPH_ASSERT(ptr);
        return (*ptr)(std::forward<TArgs>(args)...);
    }

private:
    template <typename T2>
    INLINE void copyBlock(const SharedPtr<T2>& other) {
        if (other.block) {
            SPH_ASSERT(ptr != nullptr);
            block = other.block;
            block->increaseUseCnt();
            block->increaseWeakCnt();
        } else {
            block = nullptr;
        }
    }
};


template <typename T>
bool operator==(const SharedPtr<T>& ptr, std::nullptr_t) {
    return !ptr;
}

template <typename T>
bool operator==(std::nullptr_t, const SharedPtr<T>& ptr) {
    return !ptr;
}

template <typename T>
bool operator!=(const SharedPtr<T>& ptr, std::nullptr_t) {
    return bool(ptr);
}

template <typename T>
bool operator!=(std::nullptr_t, const SharedPtr<T>& ptr) {
    return bool(ptr);
}

template <typename T>
bool operator==(const SharedPtr<T>& ptr1, const SharedPtr<T>& ptr2) {
    return ptr1.get() == ptr2.get();
}

template <typename T>
bool operator!=(const SharedPtr<T>& ptr1, const SharedPtr<T>& ptr2) {
    return ptr1.get() != ptr2.get();
}

template <typename T>
bool operator<(const SharedPtr<T>& ptr1, const SharedPtr<T>& ptr2) {
    return ptr1.get() < ptr2.get();
}

template <typename T>
class WeakPtr {
private:
    Detail::ControlBlockHolder* block;

public:
    WeakPtr()
        : block(nullptr) {}

    WeakPtr(const WeakPtr& other)
        : block(other.block) {
        if (block) {
            block->increaseWeakCnt();
        }
    }

    template <typename T2>
    WeakPtr(const WeakPtr<T2>& other)
        : block(other.block) {
        if (block) {
            block->increaseWeakCnt();
        }
    }

    template <typename T2>
    WeakPtr(const SharedPtr<T2>& ptr)
        : block(ptr.block) {
        if (block) {
            block->increaseWeakCnt();
        }
    }

    WeakPtr(std::nullptr_t)
        : block(nullptr) {}

    ~WeakPtr() {
        this->reset();
    }

    WeakPtr& operator=(const WeakPtr& other) {
        this->reset();
        block = other.block;
        if (block) {
            block->increaseWeakCnt();
        }
        return *this;
    }

    template <typename T2>
    WeakPtr& operator=(const WeakPtr<T2>& other) {
        this->reset();
        block = other.block;
        if (block) {
            block->increaseWeakCnt();
        }
        return *this;
    }

    WeakPtr& operator=(std::nullptr_t) {
        this->reset();
        return *this;
    }

    SharedPtr<T> lock() const {
        if (block && block->increaseUseCntIfNonzero()) {
            SharedPtr<T> ptr;
            ptr.block = block;
            ptr.ptr = static_cast<T*>(block->getPtr());
            block->increaseWeakCnt();
            return ptr;
        } else {
            return nullptr;
        }
    }

    INLINE void reset() {
        if (block) {
            block->decreaseWeakCnt();
            block = nullptr;
        }
    }

    INLINE Size getUseCount() const {
        if (!block) {
            return 0;
        } else {
            return block->getUseCount();
        }
    }

    INLINE explicit operator bool() const {
        return this->getUseCount() > 0;
    }

    INLINE bool operator!() const {
        return this->getUseCount() == 0;
    }
};

template <typename T, typename... TArgs>
INLINE SharedPtr<T> makeShared(TArgs&&... args) {
    return SharedPtr<T>(alignedNew<T>(std::forward<TArgs>(args)...));
}

template <typename T>
class ShareFromThis {
private:
    WeakPtr<T> ptr;

public:
    using SHARE_FROM_THIS_TAG = void;

    void setWeakPtr(const WeakPtr<T>& weakPtr) {
        ptr = weakPtr;
    }

    SharedPtr<T> sharedFromThis() {
        SharedPtr<T> sharedPtr = ptr.lock();
        SPH_ASSERT(sharedPtr);
        return sharedPtr;
    }

    WeakPtr<T> weakFromThis() {
        return ptr;
    }
};

/// \todo this is a weird solution, it must be doable with more standard approach

template <typename T, typename TEnabler = void>
struct IsShareFromThis {
    static constexpr bool value = false;
};
template <typename T>
struct IsShareFromThis<T, typename T::SHARE_FROM_THIS_TAG> {
    static constexpr bool value = true;
};

template <typename T>
std::enable_if_t<IsShareFromThis<T>::value> setSharedFromThis(const SharedPtr<T>& ptr) {
    ptr->setWeakPtr(ptr);
}

template <typename T>
std::enable_if_t<!IsShareFromThis<T>::value> setSharedFromThis(const SharedPtr<T>& UNUSED(ptr)) {
    // do nothing
}

NAMESPACE_SPH_END
