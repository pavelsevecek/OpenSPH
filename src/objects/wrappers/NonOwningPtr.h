#pragma once

/// Lifetime management of objects.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

class Observable;

struct ControlBlock : public Noncopyable {
    Observable* parent;
    std::atomic<int> referenceCnt;

    ControlBlock(Observable* parent)
        : parent(parent)
        , referenceCnt(1) {}
};

class NonOwningBase : public Object {
protected:
    ControlBlock* block = nullptr;

    /// Increase the number of references
    void incRefCnt() {
        if (!block) {
            // not counting anything, don't do anything
            return;
        }
        ++block->referenceCnt;
    }

    /// Decrease the number of references, destroy the control block if nothing references it anymore
    void decRefCnt() {
        if (!block) {
            // not counting anything, don't do anything
            return;
        }
        --block->referenceCnt;
        if (block->referenceCnt == 0) {
            // nothing else is holding reference to the block, safe to delete
            delete block;
            block = nullptr;
        }
    }
};


/// Smart pointer that references object without taking ownership and without a need for that object to be
/// referenced by std::shared_ptr. It is always initialized to nullptr and when the referenced object is
/// destroyed, this pointer (and all other non-owning pointers referencing the object) are set to nullptr.
template <typename T>
class NonOwningPtr : public NonOwningBase {
    template <typename>
    friend class NonOwningPtr;
    static_assert(std::is_base_of<Observable, T>::value,
                  "Non-owning pointer can be only used for Observable types.");

private:
    T* ptr = nullptr;

    INLINE bool isValid() const { return block && block->parent; }

public:
    NonOwningPtr() = default;

    ~NonOwningPtr() {
        decRefCnt();
    }

    NonOwningPtr(const std::nullptr_t&) {}

    /// Copy constructor from other non-owning pointer, adds a reference to parent observable.
    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    NonOwningPtr(const NonOwningPtr<T2>& other)
        : ptr(other.ptr) {
        this->block = other.block;
        incRefCnt();
    }

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    NonOwningPtr(T2* other)
        : ptr(other) {
        this->block = other->block;
        incRefCnt();
    }

    /// Assings nullptr, removes reference from parent observable.
    NonOwningPtr& operator=(const std::nullptr_t&) {
        decRefCnt();
        ptr   = nullptr;
        block = nullptr;
        return *this;
    }

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    NonOwningPtr& operator=(T2* parent) {
        if (ptr == parent) {
            // assigning the same, don't add reference twice
            return *this;
        }
        decRefCnt();
        ptr   = parent;
        block = parent->block;
        incRefCnt();
        return *this;
    }

    template <typename T2, typename = std::enable_if_t<std::is_convertible<T2*, T*>::value>>
    NonOwningPtr& operator=(const NonOwningPtr<T2>& other) {
        if (ptr == other.ptr) {
            // assigning the same, don't add reference twice
            return *this;
        }
        decRefCnt();
        ptr   = other.ptr;
        block = other.block;
        incRefCnt();
        return *this;
    }

    /// Implicit conversion to other pointers
    template <typename T2, typename = std::enable_if_t<std::is_convertible<T*, T2*>::value>>
    operator NonOwningPtr<T2>() {
        return NonOwningPtr<T2>(static_cast<T2*>(ptr));
    }

    /// Implicit conversion to other pointers
    template <typename T2, typename = std::enable_if_t<std::is_convertible<T*, T2*>::value>>
    operator const NonOwningPtr<T2>() const {
        return NonOwningPtr<T2>(static_cast<T2*>(ptr));
    }

    T* get() {
        if (isValid()) {
            return ptr;
        }
        return nullptr;
    }

    const T* get() const {
        if (isValid()) {
            return ptr;
        }
        return nullptr;
    }

    T* operator->() {
        ASSERT(isValid());
        return ptr;
    }

    const T* operator->() const {
        ASSERT(isValid());
        return ptr;
    }

    bool operator!() const { return !isValid(); }

    explicit operator bool() const { return isValid(); }
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
class Observable : public Polymorphic, public NonOwningBase {
    template <typename>
    friend class NonOwningPtr;

public:
    /// Control block is always created by parent observable (but can be destroyed by parent or any non-owning
    /// ptr, whatever is destroyed last)
    Observable() {
        ASSERT(!block);
        block = new ControlBlock(this);
    }

    ~Observable() {
        block->parent = nullptr;
        decRefCnt();
    }

    /// Returns the number of non-owning pointers referencing this observable. This object itself does NOT
    /// count as a reference.
    int getReferenceCnt() const { return block->referenceCnt - 1; }
};

NAMESPACE_SPH_END
