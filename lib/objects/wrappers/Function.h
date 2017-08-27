#pragma once

/// \file Function.h
/// \brief Generic wrappers of lambdas, functors and other callables
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/SharedPtr.h"

NAMESPACE_SPH_BEGIN

namespace Detail {
    template <typename TSignature>
    class Callable;

    template <typename TReturn, typename... TArgs>
    class Callable<TReturn(TArgs...)> : public Polymorphic {
    public:
        virtual TReturn operator()(TArgs&&... args) const = 0;
    };
}

template <typename TSignature>
class Function;

/// \brief Wrapper of callable objects.
///
/// Function is copyable, having a pointer semantics. Copied object reference the same callable.
template <typename TReturn, typename... TArgs>
class Function<TReturn(TArgs...)> {
private:
    SharedPtr<Detail::Callable<TReturn(TArgs...)>> holder;

    template <typename TFunctor>
    class FunctorCallable : public Detail::Callable<TReturn(TArgs...)> {
    private:
        TFunctor functor;

    public:
        FunctorCallable(TFunctor&& functor)
            : functor(std::forward<TFunctor>(functor)) {}

        virtual TReturn operator()(TArgs&&... args) const override {
            return functor(std::forward<TArgs>(args)...);
        }
    };

public:
    Function() = default;

    /// \brief Creates a function given a callable object.
    ///
    /// The functor is passed by value on purpose, we do not want to store references in the function. It also
    /// helps avoiding calling this constructor instead of copy/move constructors.
    template <typename TFunctor>
    Function(TFunctor functor)
        : holder(makeShared<FunctorCallable<TFunctor>>(std::move(functor))) {}

    Function(const Function& other)
        : holder(other.holder) {}

    Function(Function&& other)
        : holder(std::move(other.holder)) {}

    template <typename TFunctor>
    Function& operator=(TFunctor functor) {
        holder = makeShared<FunctorCallable<TFunctor>>(std::move(functor));
        return *this;
    }

    Function& operator=(const Function& other) {
        holder = other.holder;
        return *this;
    }

    Function& operator=(Function&& other) {
        holder = std::move(other.holder);
        return *this;
    }


    /// Calls the function, given argument list.
    TReturn operator()(TArgs&&... args) const {
        ASSERT(holder);
        return (*holder)(std::forward<TArgs>(args)...);
    }

    INLINE explicit operator bool() const {
        return bool(holder);
    }
};

NAMESPACE_SPH_END
