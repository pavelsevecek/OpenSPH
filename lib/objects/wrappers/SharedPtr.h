#pragma once

/// \file AutoPtr.h
/// \brief Simplified implementation of std::unique_ptr, using only default deleter.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

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
            ASSERT(cnt > 0);
            return cnt;
        }

        INLINE int getUseCount() const {
            return useCnt;
        }

        INLINE int increaseWeakCnt() {
            const int cnt = ++weakCnt;
            ASSERT(cnt > 0);
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
            ASSERT(cnt >= 0);
            if (cnt == 0) {
                this->deletePtr();
            }
        }

        INLINE void decreaseWeakCnt() {
            const int cnt = --weakCnt;
            ASSERT(cnt >= 0);
            if (cnt == 0) {
                this->deleteBlock();
            }
        }

        virtual void* getPtr() = 0;

        virtual void deletePtr() = 0;

        INLINE void deleteBlock() {
            delete this;
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
            ASSERT(ptr);
            return ptr;
        }

        virtual void deletePtr() override {
            delete ptr;
        }
    };
}

template <typename T>
class SharedPtr {
    template <typename>
    friend class SharedPtr;
    template <typename>
    friend class WeakPtr;

private:
    T* ptr;
    Detail::ControlBlockHolder* block;

public:
    SharedPtr()
        : ptr(nullptr)
        , block(nullptr) {}

    explicit SharedPtr(T* ptr)
        : ptr(ptr) {
        if (ptr) {
            block = new Detail::ControlBlock<T>(ptr);
        } else {
            block = nullptr;
        }
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
        ASSERT(ptr);
        return ptr;
    }

    INLINE T& operator*() const {
        ASSERT(ptr);
        return *ptr;
    }

    INLINE explicit operator bool() const {
        return ptr != nullptr;
    }

    INLINE bool operator!() const {
        return ptr == nullptr;
    }

    INLINE T* get() const {
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

    INLINE Size getUseCount() {
        if (!block) {
            return 0;
        } else {
            return block->getUseCount();
        }
    }

private:
    template <typename T2>
    INLINE void copyBlock(const SharedPtr<T2>& other) {
        if (other.block) {
            ASSERT(ptr != nullptr);
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

    SharedPtr<T> lock() {
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

    INLINE Size getUseCount() {
        if (!block) {
            return 0;
        } else {
            return block->getUseCount();
        }
    }
};

template <typename T, typename... TArgs>
INLINE SharedPtr<T> makeShared(TArgs&&... args) {
    return SharedPtr<T>(new T(std::forward<TArgs>(args)...));
}


NAMESPACE_SPH_END
