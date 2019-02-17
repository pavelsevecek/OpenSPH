#pragma once

/// \file AutoPtr.h
/// \brief Simplified implementation of std::unique_ptr, using only default deleter.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/wrappers/AlignedStorage.h"
#include "objects/wrappers/RawPtr.h"

NAMESPACE_SPH_BEGIN

/// \brief Wrapper of pointer that deletes the resource from destructor.
template <typename T>
class AutoPtr {
    template <typename>
    friend class AutoPtr;

private:
    T* ptr;

public:
    INLINE AutoPtr()
        : ptr(nullptr) {}

    INLINE AutoPtr(std::nullptr_t)
        : ptr(nullptr) {}

    INLINE explicit AutoPtr(T* ptr)
        : ptr(ptr) {}

    INLINE AutoPtr(const AutoPtr& other) = delete;

    template <typename T2>
    INLINE AutoPtr(AutoPtr<T2>&& other)
        : ptr(other.ptr) {
        other.ptr = nullptr;
    }

    ~AutoPtr() {
        alignedDelete(ptr);
    }

    INLINE AutoPtr& operator=(const AutoPtr& other) = delete;

    template <typename T2>
    INLINE AutoPtr& operator=(AutoPtr<T2>&& other) {
        reset();
        ptr = other.ptr;
        other.ptr = nullptr;
        return *this;
    }

    INLINE T& operator*() const {
        ASSERT(ptr);
        return *ptr;
    }

    INLINE T* operator->() const {
        ASSERT(ptr);
        return ptr;
    }

    INLINE RawPtr<T> get() const {
        return ptr;
    }

    INLINE explicit operator bool() const {
        return ptr != nullptr;
    }

    INLINE bool operator!() const {
        return !ptr;
    }

    template <typename... TArgs>
    INLINE decltype(auto) operator()(TArgs&&... args) const {
        ASSERT(ptr);
        return (*ptr)(std::forward<TArgs>(args)...);
    }

    INLINE void reset() {
        alignedDelete(ptr);
        ptr = nullptr;
    }

    INLINE T* release() {
        T* resource = ptr;
        ptr = nullptr;
        return resource;
    }

    INLINE void swap(AutoPtr& other) {
        std::swap(ptr, other.ptr);
    }
};

template <typename T>
bool operator==(const AutoPtr<T>& ptr, std::nullptr_t) {
    return !ptr;
}

template <typename T>
bool operator==(std::nullptr_t, const AutoPtr<T>& ptr) {
    return !ptr;
}

template <typename T>
bool operator!=(const AutoPtr<T>& ptr, std::nullptr_t) {
    return bool(ptr);
}

template <typename T>
bool operator!=(std::nullptr_t, const AutoPtr<T>& ptr) {
    return bool(ptr);
}

template <typename T, typename... TArgs>
INLINE AutoPtr<T> makeAuto(TArgs&&... args) {
    return AutoPtr<T>(alignedNew<T>(std::forward<TArgs>(args)...));
}

/// \brief Performs a dynamic_cast on an AutoPtr, moving the ownership of the resource to the created object.
///
/// If the dynamic type of given pointer is not T1 (or derived from T1), the function return nullptr and the
/// resource remains owned by the input \ref AutoPtr (no move is performed).
template <typename T1, typename T2>
INLINE AutoPtr<T1> dynamicCast(AutoPtr<T2>&& source) {
    if (RawPtr<T1> ptr = dynamicCast<T1>(source.get())) {
        source.release();
        return AutoPtr<T1>(ptr.get());
    } else {
        return nullptr;
    }
}

NAMESPACE_SPH_END

namespace std {
template <typename T>
void swap(Sph::AutoPtr<T>& p1, Sph::AutoPtr<T>& p2) {
    p1.swap(p2);
}
} // namespace std
