#pragma once

/// \file Algorithm.h
/// \brief Collection of random utilities for iterators and ranges
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Traits.h"
#include "math/MathUtils.h"
#include <initializer_list>
#include <type_traits>

NAMESPACE_SPH_BEGIN

template <typename TRange, typename T>
auto find(TRange&& range, const T& value) {
    for (auto iter = range.begin(); iter != range.end(); ++iter) {
        if (*iter == value) {
            return iter;
        }
    }
    return range.end();
}

template <typename TRange, typename TPredicate>
auto findIf(TRange&& range, TPredicate&& predicate) {
    for (auto iter = range.begin(); iter != range.end(); ++iter) {
        if (predicate(*iter)) {
            return iter;
        }
    }
    return range.end();
}

template <typename TRange, typename T>
bool contains(TRange&& range, const T& value) {
    return find(range, value) != range.end();
}

template <typename TRange, typename TPredicate>
int countIf(const TRange& range, TPredicate&& predicate) {
    Size cnt = 0;
    for (const auto& v : range) {
        const bool match = predicate(v);
        cnt += int(match);
    }
    return cnt;
}

template <typename TRange, typename TPredicate>
bool allMatching(const TRange& range, TPredicate&& predicate) {
    for (const auto& v : range) {
        if (!predicate(v)) {
            return false;
        }
    }
    return true;
}

template <typename TRange, typename TPredicate>
bool anyMatching(const TRange& range, TPredicate&& functor) {
    for (const auto& v : range) {
        if (functor(v)) {
            return true;
        }
    }
    return false;
}

/// Returns true if all elements in the range are unique, i.e. the range does not contain any
/// value more than once.
template <typename TRange, typename = void>
bool allUnique(const TRange& range) {
    // stupid O(N^2) solution
    for (auto iter1 = range.begin(); iter1 != range.end(); ++iter1) {
        auto iter2 = iter1;
        ++iter2;
        for (; iter2 != range.end(); ++iter2) {
            if (*iter2 == *iter1) {
                return false;
            }
        }
    }
    return true;
}

template <typename T>
bool allUnique(const std::initializer_list<T>& range) {
    return allUnique<std::initializer_list<T>, void>(range);
}

/// Returns true if two ranges have at least one element with the same value.
template <typename TRange1, typename TRange2>
bool anyCommon(const TRange1& range1, const TRange2& range2) {
    for (const auto& v1 : range1) {
        for (const auto& v2 : range2) {
            if (v1 == v2) {
                return true;
            }
        }
    }
    return false;
}

template <typename TRange, typename TInitial>
std::decay_t<typename TRange::Type> accumulate(const TRange& range, const TInitial initial) {
    std::decay_t<typename TRange::Type> sum = initial;
    for (const auto& v : range) {
        sum += v;
    }
    return sum;
}

template <typename TRange>
auto findMax(const TRange& range) {
    auto iter = range.begin();
    auto last = range.end();
    if (iter == last) {
        return iter;
    }
    auto maxIter = iter;
    ++iter;
    for (; iter != last; ++iter) {
        if (*iter > *maxIter) {
            maxIter = iter;
        }
    }
    return maxIter;
}

template <typename TRange>
auto findMin(const TRange& range) {
    auto iter = range.begin();
    auto last = range.end();
    if (iter == last) {
        return iter;
    }
    auto minIter = iter;
    ++iter;
    for (; iter != last; ++iter) {
        if (*iter < *minIter) {
            minIter = iter;
        }
    }
    return minIter;
}

/// Checks if two ranges differ no less than given eps.
template <typename TRange1,
    typename TRange2,
    typename = std::enable_if_t<IsRange<TRange1>::value && IsRange<TRange2>::value>>
bool almostEqual(const TRange1& range1, const TRange2& range2, const Float& eps) {
    if (range1.size() != range2.size()) {
        return false;
    }

    auto iter1 = range1.begin();
    const auto end1 = range1.end();
    auto iter2 = range2.begin();

    for (; iter1 != end1; ++iter1, ++iter2) {
        if (!almostEqual(*iter1, *iter2, eps)) {
            return false;
        }
    }
    return true;
}

NAMESPACE_SPH_END
