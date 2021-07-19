#pragma once

/// \file CallbackSet.h
/// \brief Container of callbacks.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/containers/Array.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/SharedToken.h"

NAMESPACE_SPH_BEGIN

template <typename TSignature>
class CallbackSet;

template <typename... TArgs>
class CallbackSet<void(TArgs...)> {
public:
    struct Callback {
        Function<void(TArgs...)> functor;
        WeakToken owner;
    };

private:
    SharedPtr<Array<Callback>> callbacks;

public:
    CallbackSet()
        : callbacks(makeShared<Array<Callback>>()) {}

    void insert(const SharedToken& owner, const Function<void(TArgs...)>& functor) {
        if (owner) {
            callbacks->push(Callback{ functor, owner });
        }
    }

    /// Calls all registered callbacks.
    void operator()(TArgs... args) const {
        for (const Callback& callback : *callbacks) {
            if (auto owner = callback.owner.lock()) {
                callback.functor(args...);
            }
        }
    }

    Iterator<const Callback> begin() const {
        return callbacks->begin();
    }

    Iterator<const Callback> end() const {
        return callbacks->end();
    }

    Size size() const {
        return callbacks->size();
    }

    bool empty() const {
        return callbacks->empty();
    }
};

/// Convenience specialization for Function
template <typename... TArgs>
class CallbackSet<Function<void(TArgs...)>> : public CallbackSet<void(TArgs...)> {};

NAMESPACE_SPH_END
