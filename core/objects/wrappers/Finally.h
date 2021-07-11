#pragma once

/// \file Finally.h
/// \brief Wraps a functor and executes it once the wrapper goes out of scope.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Assert.h"
#include "common/Traits.h"

NAMESPACE_SPH_BEGIN

template <typename TFunctor>
class Finally {
private:
    TFunctor functor;
    bool moved = false;

public:
    Finally(TFunctor&& functor)
        : functor(std::forward<TFunctor>(functor)) {}

    Finally(Finally&& other)
        : functor(std::move(other.functor)) {
        other.moved = true;
    }

    ~Finally() {
        if (!moved) {
            functor();
        }
    }
};

/// Returns Finally object utilizing type deduction
template <typename TFunctor>
Finally<std::decay_t<TFunctor>> finally(TFunctor&& functor) {
    return Finally<std::decay_t<TFunctor>>(std::forward<TFunctor>(functor));
}


NAMESPACE_SPH_END
