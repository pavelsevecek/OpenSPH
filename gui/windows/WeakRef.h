#pragma once

#include "objects/wrappers/SharedPtr.h"
#include "thread/CheckFunction.h"
#include <wx/weakref.h>

NAMESPACE_SPH_BEGIN

template <typename T>
class WeakRef {
private:
    SharedPtr<wxWeakRef<T>> ref;

public:
    WeakRef() = default;

    WeakRef(T* window) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        SPH_ASSERT(window != nullptr);
        ref = makeShared<wxWeakRef<T>>(window);
    }

    WeakRef(const WeakRef& other) = default;

    WeakRef& operator=(T* window) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        SPH_ASSERT(window != nullptr);
        ref = makeShared<wxWeakRef<T>>(window);
        return *this;
    }

    WeakRef& operator=(const WeakRef& other) = default;

    T* operator->() const {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        return ref ? *ref : nullptr;
    }

    T* get() const {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        return ref ? *ref : nullptr;
    }

    T* nonMainThreadGet() const {
        return ref ? *ref : nullptr;
    }

    operator bool() const {
        return bool(ref) && *ref;
    }

    bool operator!() const {
        return !ref || !*ref;
    }
};

NAMESPACE_SPH_END
