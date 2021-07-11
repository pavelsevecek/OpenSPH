#pragma once

/// \file ArrayUtils.h
/// \brief Utilities to simplify working with arrays
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "math/MathUtils.h"
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

/// Returns true if all elements stored in the container are unique, i.e. the container does not store any
/// value more than once.
template <typename TStorage>
bool areElementsUnique(const TStorage& container) {
    // stupid O(N^2) solution
    for (auto iter1 = container.begin(); iter1 != container.end(); ++iter1) {
        for (auto iter2 = iter1; iter2 != container.end(); ++iter2) {
            if (*iter2 == *iter1 && iter2 != iter1) {
                return false;
            }
        }
    }
    return true;
}

/// Returns true if two containers have at least one element with the same value.
template <typename TStorage1, typename TStorage2>
bool haveCommonElements(const TStorage1& c1, const TStorage2& c2) {
    for (const auto& t1 : c1) {
        for (const auto& t2 : c2) {
            if (t1 == t2) {
                return true;
            }
        }
    }
    return false;
}

NAMESPACE_SPH_END
