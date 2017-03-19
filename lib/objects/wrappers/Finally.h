#pragma once

/// Wraps a functor and executes it once the wrapper goes out of scope.

#include "core/Traits.h"
#include <functional>

NAMESPACE_SPH_BEGIN

class Finally {
protected:
    std::function<void()> functor;

public:
    /// Constructs empty finally, holds no functor.
    Finally()
        : functor(nullptr) {}

    template <typename TFunctor>
    Finally(TFunctor&& functor)
        : functor(std::forward<TFunctor>(functor)) {}

    /// Move constructor, transfers ownership of the functor.
    Finally(Finally&& other)
        : functor(std::move(other.functor)) {
        other.functor = nullptr;
    }

    Finally& operator=(Finally&& other) {
        std::swap(functor, other.functor);
        return *this;
    }

    ~Finally() {
        if (functor) {
            functor();
        }
    }
};

/// Returns Finally object utilizing type deduction
template <typename TFunctor>
Finally finally(TFunctor&& functor) {
    return Finally(std::forward<TFunctor>(functor));
}

NAMESPACE_SPH_END
