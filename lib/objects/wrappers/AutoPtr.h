#pragma once

/// Simplified implementation of std::unique_ptr, using only default deleter.
#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

template <typename T>
class AutoPtr {
private:
    T* ptr;

public:
    INLINE AutoPtr()
        : ptr(nullptr) {}

    INLINE explicit AutoPtr(T* ptr)
        : ptr(ptr) {}

    INLINE AutoPtr(const AutoPtr& other) = delete;

    INLINE AutoPtr(AutoPtr&& other)
        : ptr(other.ptr) {
        other.ptr = nullptr;
    }

    ~AutoPtr() {
        delete ptr;
    }

    INLINE AutoPtr& operator=(const AutoPtr& other) = delete;

    INLINE AutoPtr& operator=(AutoPtr&& other) {
        std::swap(ptr, other.ptr);
        return *this;
    }

    INLINE T& operator*() {
        ASSERT(ptr);
        return *ptr;
    }

    INLINE const T& operator*() const {
        ASSERT(ptr);
        return *ptr;
    }

    INLINE T* operator->() {
        ASSERT(ptr);
        return ptr;
    }

    INLINE const T* operator->() const {
        ASSERT(ptr);
        return ptr;
    }

    INLINE explicit operator bool() const {
        return ptr != nullptr;
    }

    INLINE bool operator!() const {
        return !ptr;
    }
};

template <typename T>
INLINE bool operator==(const AutoPtr<T>& ptr, std::nullptr_t) {
    return ptr != nullptr;
}

template <typename T>
INLINE bool operator==(std::nullptr_t, const AutoPtr<T>& ptr) {
    return ptr != nullptr;
}

template <typename T>
INLINE bool operator!=(const AutoPtr<T>& ptr, std::nullptr_t) {
    return ptr == nullptr;
}

template <typename T>
INLINE bool operator!=(std::nullptr_t, const AutoPtr<T>& ptr) {
    return ptr == nullptr;
}

template <typename T, typename... TArgs>
INLINE AutoPtr<T> makeAuto(TArgs&&... args) {
    return AutoPtr<T>(new T(std::forward<TArgs>(args)...));
}

NAMESPACE_SPH_END
