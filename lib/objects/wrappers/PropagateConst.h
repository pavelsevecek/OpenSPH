#pragma once

/// \file PropagateConst.h
/// \brief Wrapper of another pointer-like object with reference-like const correctness.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/wrappers/RawPtr.h"

NAMESPACE_SPH_BEGIN

/// \brief Const-propagating wrapper for object with point semantics.
///
/// It treats the wrapped pointer as a pointer to const when accessed through a const access path.
template <typename TPtr>
class PropagateConst {
private:
    TPtr ptr;

    using TRef = decltype(*std::declval<TPtr>());
    static_assert(std::is_reference<TRef>::value, "Must be a reference");

    using TValue = std::decay_t<TRef>;
    using TConstRef = std::add_const_t<TValue>&;

public:
    PropagateConst() = default;

    PropagateConst(TPtr&& ptr)
        : ptr(std::move(ptr)) {}

    PropagateConst(PropagateConst&& other)
        : ptr(std::move(other.ptr)) {}

    PropagateConst& operator=(TPtr&& newPtr) {
        ptr = std::move(newPtr);
        return *this;
    }

    TRef operator*() {
        return *ptr;
    }

    TConstRef operator*() const {
        return *ptr;
    }

    RawPtr<TValue> operator->() {
        return ptr.get();
    }

    RawPtr<const TValue> operator->() const {
        return ptr.get();
    }
};

NAMESPACE_SPH_END
