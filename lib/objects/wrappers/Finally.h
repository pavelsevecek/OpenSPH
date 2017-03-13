#pragma once

/// Wraps a functor and executes it once the wrapper goes out of scope.

#include "common/Assert.h"

NAMESPACE_SPH_BEGIN

template <typename TFunctor>
class Finally {
private:
    TFunctor functor;

public:
    Finally(TFunctor&& functor)
        : functor(std::forward<TFunctor>(functor)) {}

    ~Finally() {
        functor();
    }
};

/// Returns Finally object utilizing type deduction
template <typename TFunctor>
Finally<TFunctor> finally(TFunctor&& functor) {
    return Finally<TFunctor>(std::forward<TFinally>(functor));
}

NAMESPACE_SPH_END
