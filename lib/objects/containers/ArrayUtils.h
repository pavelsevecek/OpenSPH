#pragma once

/// Utilities to simplify working with arrays.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/containers/ArrayView.h"
#include "objects/containers/Tuple.h"

NAMESPACE_SPH_BEGIN

template <typename U, typename T, typename TFunctor, typename TComparator>
Iterator<T> findByComparator(ArrayView<T> container,
    TFunctor&& functor,
    const U defaultValue,
    TComparator&& comparator) {
    U maxValue = defaultValue;
    Iterator<T> maxIter = container.begin();
    for (Iterator<T> iter = container.begin(); iter != container.end(); ++iter) {
        U value = functor(*iter);
        if (comparator(value, maxValue)) {
            maxIter = iter;
            maxValue = value;
        }
    }
    return maxIter;
}

template <typename U, typename T, typename TFunctor>
Iterator<T> findByMaximum(ArrayView<T> container, TFunctor&& functor) {
    return findByComparator<U>(
        container, std::forward<TFunctor>(functor), U(-INFTY), [](const U& v1, const U& v2) {
            return v1 > v2;
        });
}


template <typename U, typename T, typename TFunctor>
Iterator<T> findByMinimum(ArrayView<T> container, TFunctor&& functor) {
    return findByComparator<U>(
        container, std::forward<TFunctor>(functor), U(INFTY), [](const U& v1, const U& v2) {
            return v1 < v2;
        });
}

/// Returns first the lower iterator
/// \todo ignore same iterator (inner = outer?) flag?
template <typename U, typename T, typename TFunctor, typename TComparator>
Tuple<Iterator<T>, Iterator<T>> findPairByComparator(ArrayView<T> container,
    TFunctor&& functor,
    const U defaultValue,
    TComparator&& comparator) {
    Iterator<T> maxOuterIter = container.begin();
    Iterator<T> maxInnerIter = container.end();
    U maxValue = defaultValue;
    for (Iterator<T> outerIter = container.begin(); outerIter != container.end(); ++outerIter) {
        for (Iterator<T> innerIter = container.begin(); innerIter != container.end(); ++innerIter) {
            if (outerIter == innerIter) {
                continue;
            }
            U value = functor(*innerIter, *outerIter);
            if (comparator(value, maxValue)) {
                maxOuterIter = outerIter;
                maxInnerIter = innerIter;
                maxValue = value;
            }
        }
    }
    return maxInnerIter > maxOuterIter ? makeTuple(maxOuterIter, maxInnerIter)
                                       : makeTuple(maxInnerIter, maxOuterIter);
}

template <typename U, typename T, typename TFunctor>
Tuple<Iterator<T>, Iterator<T>> findPairByMaximum(ArrayView<T> container, TFunctor&& functor) {
    return findPairByComparator<U>(
        container, std::forward<TFunctor>(functor), U(-INFTY), [](const U& v1, const U& v2) {
            return v1 > v2;
        });
}

template <typename U, typename T, typename TFunctor>
Tuple<Iterator<T>, Iterator<T>> findPairByMinimum(ArrayView<T> container, TFunctor&& functor) {
    return findPairByComparator<U>(
        container, std::forward<TFunctor>(functor), U(INFTY), [](const U& v1, const U& v2) {
            return v1 < v2;
        });
}

template <typename TStorage, typename TFunctor>
int getCountMatching(const TStorage& container, TFunctor&& functor) {
    int cnt = 0;
    for (const auto& t : container) {
        if (functor(t)) {
            cnt++;
        }
    }
    return cnt;
}

template <typename TStorage, typename TFunctor>
bool areAllMatching(const TStorage& container, TFunctor&& functor) {
    for (const auto& t : container) {
        if (!functor(t)) {
            return false;
        }
    }
    return true;
}

template <typename TStorage, typename TFunctor>
bool isAnyMatching(const TStorage& container, TFunctor&& functor) {
    for (const auto& t : container) {
        if (functor(t)) {
            return true;
        }
    }
    return false;
}

NAMESPACE_SPH_END
