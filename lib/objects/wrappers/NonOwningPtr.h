#pragma once

/// Lifetime management of objects.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <memory>

NAMESPACE_SPH_BEGIN

class Observable;


/// Smart pointer that references object without taking ownership and without a need for that object to be
/// referenced by std::shared_ptr. It is always initialized to nullptr and when the referenced object is
/// destroyed, this pointer (and all other non-owning pointers referencing the object) are set to nullptr.
template <typename T>
class NonOwningPtr : public Object {
    template <typename>
    friend class NonOwningPtr;
    friend class Observable;

    static_assert(std::is_base_of<Observable, T>::value,
                  "Non-owning pointer can be only used for Observable types.");

private:
    T* ptr = nullptr;
    std::shared_ptr<bool> valid;

    INLINE bool isValid() const { return valid && *valid; }

public:
    NonOwningPtr() = default;

    NonOwningPtr(const std::nullptr_t&) {}

    /// Copy constructor from other non-owning pointer, adds a reference to parent observable.
    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    NonOwningPtr(const NonOwningPtr<T2>& other)
        : ptr(other.ptr) {
        this->valid = other.valid;
    }

    /// Copy constructor from pointer to other observable type, adds a reference to parent observable.
    template <typename T2,
              typename = std::enable_if_t<std::is_base_of<Observable, T2>::value &&
                                          std::is_convertible<T2*, T*>::value>>
    NonOwningPtr(T2* other)
        : ptr(other) {
        this->valid = other->valid;
    }

    /// Assings nullptr, removes reference from parent observable.
    NonOwningPtr& operator=(const std::nullptr_t&) {
        this->ptr = nullptr;
        this->valid.reset();
        return *this;
    }

    /// Copy assignment from pointer to other observable type, adds a reference to parent observable.
    template <typename T2,
              typename = std::enable_if_t<std::is_base_of<Observable, T2>::value &&
                                          std::is_convertible<T2*, T*>::value>>
    NonOwningPtr& operator=(T2* parent) {
        this->ptr   = parent;
        this->valid = parent->valid;
        return *this;
    }

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    NonOwningPtr& operator=(const NonOwningPtr<T2>& other) {
        this->ptr   = other.ptr;
        this->valid = other.valid;
        return *this;
    }

    /// Implicit conversion to other non-owning pointers
    template <typename T2, typename = std::enable_if_t<std::is_convertible<T*, T2*>::value>>
    operator NonOwningPtr<T2>() {
        return NonOwningPtr<T2>(static_cast<T2*>(this->ptr));
    }

    /// Implicit conversion to other non-owning pointers, const version.
    template <typename T2, typename = std::enable_if_t<std::is_convertible<T*, T2*>::value>>
    operator const NonOwningPtr<T2>() const {
        return NonOwningPtr<T2>(static_cast<T2*>(this->ptr));
    }

    /// Returns stored resource. If the pointer is no longer valid, returns nullptr.
    T* get() {
        if (this->isValid()) {
            return this->ptr;
        }
        return nullptr;
    }

    /// Returns stored resource, const version. If the pointer is no longer valid, returns nullptr.
    const T* get() const {
        if (this->isValid()) {
            return this->ptr;
        }
        return nullptr;
    }

    T* operator->() {
        ASSERT(this->isValid());
        return this->ptr;
    }

    const T* operator->() const {
        ASSERT(this->isValid());
        return this->ptr;
    }

    bool operator!() const { return !this->isValid(); }

    explicit operator bool() const { return this->isValid(); }
};

template <typename T1, typename T2>
bool operator==(const NonOwningPtr<T1>& p1, const T2* p2) {
    return p1.get() == p2;
}

template <typename T>
bool operator==(const NonOwningPtr<T>& p1, const std::nullptr_t&) {
    return p1.get() == nullptr;
}

template <typename T1, typename T2>
bool operator==(const T1* p1, const NonOwningPtr<T2>& p2) {
    return p1 == p2.get();
}

template <typename T1, typename T2>
NonOwningPtr<T1> nonOwningDynamicCast(NonOwningPtr<T2> ptr) {
    return NonOwningPtr<T1>(dynamic_cast<T1*>(ptr.get()));
}


/// Base class for all objects that can be referenced by non-owning pointers.
class Observable : public Polymorphic {
    template <typename>
    friend class NonOwningPtr;

protected:
    std::shared_ptr<bool> valid;

public:
    Observable() { valid = std::make_shared<bool>(true); }

    ~Observable() { *valid = false; }

    /// Returns the number of non-owning pointers referencing this observable. This object itself does NOT
    /// count as a reference.
    int getReferenceCnt() const { return valid.use_count() - 1; }
};

NAMESPACE_SPH_END
