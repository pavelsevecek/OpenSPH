#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN


enum class IterableType {
    FIRST_ORDER,  ///< Quantities evolved in time using its first derivatives
    SECOND_ORDER, ///< Quantities evolved in time using its first and second derivatives derivatives
    ALL,
};


/// Auxiliary structure holding together references to quantities related by temporal derivatives.
template <typename TValue, IterableType Type>
struct IterableWrapper;

template <typename TValue>
struct IterableWrapper<TValue, IterableType::FIRST_ORDER> {
    Array<TValue>& f;
    Array<TValue>& df;

  /*  IterableWrapper(const IterableWrapper& other)
        : f(other.f)
        , df(other.df) {}*/
};

template <typename TValue>
struct IterableWrapper<TValue, IterableType::SECOND_ORDER> {
    Array<TValue>& f;
    Array<TValue>& df;
    Array<TValue>& d2f;

    /*IterableWrapper(const IterableWrapper& other)
        : f(other.f)
        , df(other.df)
        , d2f(other.d2f) {}*/
};

template <IterableType Type>
struct Iterables {
    Array<IterableWrapper<Float, Type>> scalars;
    Array<IterableWrapper<Vector, Type>> vectors;

   /* Iterables(const Iterables& other)
        : scalars(other.scalars.clone())
        , vectors(other.vectors.clone()) {}

    template<typename TScalars, typename TVectors>
    Iterables(TScalars&& scalars, TVectors&& vectors) : scalars(std::forward<TScalars>(scalars)),vectors(std::forward<TVectors>(vectors)) {}*/
};


template <>
struct Iterables<IterableType::ALL> {
    Array<Array<Float>>& scalars;
    Array<Array<Vector>>& vectors;
};


template <IterableType Type>
struct StorageIterator;

template <>
struct StorageIterator<IterableType::ALL> {
    template <typename TView, typename TFunctor>
    static void iterate(TView&& view, TFunctor&& functor) {
        for (auto& s : view.scalars) {
            functor(s);
        }
        for (auto& v : view.vectors) {
            functor(v);
        }
    }

    template <typename TView, typename TFunctor>
    static void iteratePair(TView&& view1, TView&& view2, TFunctor&& functor) {
        for (int i = 0; i < view1.scalars.size(); ++i) {
            functor(view1.scalars[i], view2.scalars[i]);
        }
        for (int i = 0; i < view1.vectors.size(); ++i) {
            functor(view1.vectors[i], view2.vectors[i]);
        }
    }
};

template <>
struct StorageIterator<IterableType::FIRST_ORDER> {
    template <typename TView, typename TFunctor>
    static void iterate(TView&& view, TFunctor&& functor) {
        Iterables<IterableType::FIRST_ORDER>& iterables =
            view.template getIterables<IterableType::FIRST_ORDER>();
        for (auto& s : iterables.scalars) {
            functor(s.f, s.df);
        }
        for (auto& v : iterables.vectors) {
            functor(v.f, v.df);
        }
    }

    template <typename TView, typename TFunctor>
    static void iteratePair(TView&& view1, TView&& view2, TFunctor&& functor) {
        Iterables<IterableType::FIRST_ORDER>& iterables1 =
            view1.template getIterables<IterableType::FIRST_ORDER>();
        Iterables<IterableType::FIRST_ORDER>& iterables2 =
            view2.template getIterables<IterableType::FIRST_ORDER>();
        ASSERT(iterables1.scalars.size() == iterables2.scalars.size());
        for (int i = 0; i < iterables1.scalars.size(); ++i) {
            functor(iterables1.scalars[i], iterables2.scalars[i]);
        }
        ASSERT(iterables1.vectors.size() == iterables2.vectors.size());
        for (int i = 0; i < iterables2.vectors.size(); ++i) {
            functor(iterables1.vectors[i], iterables2.vectors[i]);
        }
    }
};

template <>
struct StorageIterator<IterableType::SECOND_ORDER> {
    template <typename TView, typename TFunctor>
    static void iterate(TView&& view, TFunctor&& functor) {
        Iterables<IterableType::SECOND_ORDER>& iterables =
            view.template getIterables<IterableType::SECOND_ORDER>();
        for (auto& s : iterables.scalars) {
            functor(s.f, s.df, s.d2f);
        }
        for (auto& v : iterables.vectors) {
            functor(v.f, v.df, v.d2f);
        }
    }

    template <typename TView, typename TFunctor>
    static void iteratePair(TView&& view1, TView&& view2, TFunctor&& functor) {
        Iterables<IterableType::SECOND_ORDER>& iterables1 =
            view1.template getIterables<IterableType::SECOND_ORDER>();
        Iterables<IterableType::SECOND_ORDER>& iterables2 =
            view2.template getIterables<IterableType::SECOND_ORDER>();
        ASSERT(iterables1.scalars.size() == iterables2.scalars.size());
        for (int i = 0; i < iterables1.scalars.size(); ++i) {
            functor(iterables1.scalars[i], iterables2.scalars[i]);
        }
        ASSERT(iterables1.vectors.size() == iterables2.vectors.size());
        for (int i = 0; i < iterables2.vectors.size(); ++i) {
            functor(iterables1.vectors[i], iterables2.vectors[i]);
        }
    }
};


/// Iterate over given type of quantities and executes functor for each. The functor is used for scalars,
/// vectors and tensors, so it must be a generic lambda or a class with overloaded operator() for each
/// value type.
template <IterableType Type, typename TView, typename TFunctor>
void iterate(TView&& view, TFunctor&& functor) {
    StorageIterator<Type>::iterate(std::forward<TView>(view), std::forward<TFunctor>(functor));
}


/// Iterate over given type of quantities in two storage views and executes functor for each pair.
template <IterableType Type, typename TView, typename TFunctor>
void iteratePair(TView&& view1, TView&& view2, TFunctor&& functor) {
    StorageIterator<Type>::iteratePair(std::forward<TView>(view1),
                                       std::forward<TView>(view2),
                                       std::forward<TFunctor>(functor));
}



template<typename TValue>
struct QuantityFunction : public Object {
    /// Function call for generic iteration through all quantities
    virtual void operator()(Array<TValue>& v) = 0;

    /// Function call for iteration through 1st order quantities
    virtual void operator()(Array<TValue>& v, Array<TValue>& dv) = 0;

    virtual void operator()(Array<TValue>& v, Array<TValue>& dv, Array<TValue>& d2v) = 0;
};


NAMESPACE_SPH_END
