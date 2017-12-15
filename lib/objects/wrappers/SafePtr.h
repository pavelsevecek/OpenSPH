#pragma once

/// \file SafePtr.h
/// \brief Safer alternative to AutoPtr, throwing exception when a nullptr is dereferenced.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

class SafePtrException : public std::exception {
public:
    virtual const char* what() const noexcept {
        return "Dereferencing nullptr";
    }
};

template <typename T>
class SafePtr : public AutoPtr<T> {
public:
    INLINE T& operator*() const {
        if (!ptr) {
            throw SafePtrException();
        }
        return *ptr;
    }

    INLINE T* operator->() const {
        if (!ptr) {
            throw SafePtrException();
        }
        return ptr;
    }

    template <typename... TArgs>
    INLINE decltype(auto) operator()(TArgs&&... args) const {
        if (!ptr) {
            throw SafePtrException();
        }
        return (*ptr)(std::forward<TArgs>(args)...);
    }
};

NAMESPACE_SPH_END
