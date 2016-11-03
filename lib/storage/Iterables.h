#pragma once

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN


enum class IterableType {
    FIRST_ORDER,  ///< Quantities evolved in time using its first derivatives
    SECOND_ORDER, ///< Quantities evolved in time using its first and second derivatives derivatives
};


/// Auxiliary structure holding together references to quantities related by temporal derivatives.
template <typename TValue, IterableType Type>
struct IterableWrapper;

template <typename TValue>
struct IterableWrapper<TValue, IterableType::FIRST_ORDER> {
    Array<TValue>& f;
    Array<TValue>& df;
};

template <typename TValue>
struct IterableWrapper<TValue, IterableType::SECOND_ORDER> {
    Array<TValue>& f;
    Array<TValue>& df;
    Array<TValue>& d2f;
};

template <IterableType Type>
struct Iterables {
    Array<IterableWrapper<Float, Type>> scalars;
    Array<IterableWrapper<Vector, Type>> vectors;
};



template <IterableType Type>
struct StorageIterator;
/*
template <>
struct StorageIterator<IterableType::ALL> {
    template <typename TView, typename TFunctor>
    static void iterate(TView&& view, TFunctor&& functor) {
        Iterables<IterableType::ALL> iterables = view->template getIterables<IterableType::ALL>();
        for (auto& s : iterables.scalars) {
            functor(s);
        }
        for (auto& v : iterables.vectors) {
            functor(v);
        }
    }
};*/

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
    switch (Type) {
   /* case IterableType::ALL: {
        Iterables<Type> iterables1 = view1->template getIterables<Type>();
        Iterables<Type> iterables2 = view2->template getIterables<Type>();
        ASSERT(iterables1.scalars.size() == iterables2.scalars.size());
        for (int i = 0; i < iterables1.scalars.size(); ++i) {
            functor(iterables1.scalars[i].f, iterables2.scalars[i].f);
        }
        ASSERT(iterables1.vectors.size() == iterables2.vectors.size());
        for (int i = 0; i < iterables2.vectors.size(); ++i) {
            functor(iterables1.vectors[i].f, iterables2.vectors[i].f);
        }
        break;
    }*/
    case IterableType::FIRST_ORDER:
    case IterableType::SECOND_ORDER: {
        Iterables<Type> iterables1 = view1->template getIterables<Type>();
        Iterables<Type> iterables2 = view2->template getIterables<Type>();
        ASSERT(iterables1.scalars.size() == iterables2.scalars.size());
        for (int i = 0; i < iterables1.scalars.size(); ++i) {
            functor(iterables1.scalars[i], iterables2.scalars[i]);
        }
        ASSERT(iterables1.vectors.size() == iterables2.vectors.size());
        for (int i = 0; i < iterables2.vectors.size(); ++i) {
            functor(iterables1.vectors[i], iterables2.vectors[i]);
        }

        break;
    }
    }
}


NAMESPACE_SPH_END
