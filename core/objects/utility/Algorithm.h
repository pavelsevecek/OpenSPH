#pragma once

/// \file Algorithm.h
/// \brief Collection of random utilities for iterators and ranges
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

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

NAMESPACE_SPH_END
