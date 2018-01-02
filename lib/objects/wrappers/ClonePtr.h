#pragma once

/// \file ClonePtr.h
/// \brief Smart pointer performing cloning of stored resource rather than copying pointer
/// \author Pavel Sevecek
/// \date 2016-2018

#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

template <typename T>
class ClonePtr;

namespace Detail {
    struct Cloner : public Polymorphic {

        virtual void* clonePtr() const = 0;

        virtual AutoPtr<Cloner> cloneThis() const = 0;
    };
    template <typename T>
    class TypedCloner : public Cloner {
    private:
        T* ptr;

    public:
        TypedCloner(T* ptr)
            : ptr(ptr) {}

        virtual void* clonePtr() const override {
            if (!ptr) {
                return nullptr;
            } else {
                // clone the resource and itself
                return new T(*ptr);
            }
        }

        virtual AutoPtr<Cloner> cloneThis() const override {
            if (!ptr) {
                return nullptr;
            } else {
                return makeAuto<TypedCloner<T>>(*this);
            }
        }
    };
}

template <typename T>
class ClonePtr {
    template <typename>
    friend class ClonePtr;

private:
    AutoPtr<T> ptr;


    AutoPtr<Detail::Cloner> cloner;

    ClonePtr(AutoPtr<T>&& ptr, AutoPtr<Detail::Cloner>&& cloner)
        : ptr(std::move(ptr))
        , cloner(std::move(cloner)) {}

public:
    ClonePtr() = default;

    ClonePtr(std::nullptr_t) {}

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    explicit ClonePtr(T2* ptr)
        : ptr(ptr)
        , cloner(makeAuto<Detail::TypedCloner<T2>>(ptr)) {}

    ClonePtr(const ClonePtr& other)
        : ClonePtr(other.clone()) {}

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    ClonePtr(const ClonePtr<T2>& other)
        : ClonePtr(other.clone()) {} // delegate move constructor

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    ClonePtr(ClonePtr<T2>&& other)
        : ptr(std::move(other.ptr))
        , cloner(std::move(other.cloner)) {}

    ClonePtr& operator=(const ClonePtr& other) {
        return this->operator=(other.clone());
    }

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    ClonePtr& operator=(const ClonePtr<T2>& other) {
        return this->operator=(other.clone());
    }

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    ClonePtr& operator=(ClonePtr<T2>&& other) {
        ptr = std::move(other.ptr);
        cloner = std::move(other.cloner);
        return *this;
    }

    /// Conversion operator to AutoPtr
    operator AutoPtr<T>() && {
        return std::move(ptr);
    }

    operator AutoPtr<T>() & = delete;

    /// Explicitly create a new copy
    ClonePtr<T> clone() const {
        ASSERT(cloner);
        AutoPtr<T> cloned(static_cast<T*>(cloner->clonePtr()));
        return ClonePtr<T>(std::move(cloned), cloner->cloneThis());
    }

    T& operator*() const {
        ASSERT(ptr);
        return *ptr;
    }

    RawPtr<T> operator->() const {
        ASSERT(ptr);
        return ptr.get();
    }

    RawPtr<T> get() const {
        return ptr.get();
    }

    explicit operator bool() const {
        return bool(ptr);
    }

    bool operator!() const {
        return !ptr;
    }

    /// Compares objects rather than pointers.
    bool operator==(const ClonePtr& other) const {
        if (!*this || !other) {
            return !*this && !other; // if both are nullptr, return true
        }
        return **this == *other;
    }
};

template <typename T>
INLINE bool operator==(const ClonePtr<T>& ptr, const std::nullptr_t&) {
    return !ptr;
}

template <typename T>
INLINE bool operator==(const std::nullptr_t&, const ClonePtr<T>& ptr) {
    return !ptr;
}

template <typename T>
INLINE bool operator!=(const ClonePtr<T>& ptr, const std::nullptr_t&) {
    return (bool)ptr;
}

template <typename T>
INLINE bool operator!=(const std::nullptr_t&, const ClonePtr<T>& ptr) {
    return (bool)ptr;
}

template <typename T, typename... TArgs>
INLINE ClonePtr<T> makeClone(TArgs&&... args) {
    return ClonePtr<T>(new T(std::forward<TArgs>(args)...));
}

NAMESPACE_SPH_END
