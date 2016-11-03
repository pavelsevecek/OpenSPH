#pragma once

/// Routines for working with intervals.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/Math.h"
#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

/// Object defining 1D interval for arbitrary type. Underlying type must define operator <= and functions
/// Math::min and Math::max.
template <typename T>
class Range : public Object {
private:
    T lower;
    T upper;

public:
    INLINE Range()
        : lower(T(INFTY))
        , upper(T(-INFTY)) {}

    INLINE Range(const T& lower, const T& upper)
        : lower(lower)
        , upper(upper) {
        ASSERT(lower <= upper);
    }

    INLINE void extend(const T& value) {
        lower = Math::min(lower, value);
        upper = Math::max(upper, value);
    }

    INLINE bool contains(const T& value) const { return lower <= value && value <= upper; }

    INLINE T clamp(const T& value) const { return Math::max(lower, Math::min(value, upper)); }

    INLINE const T& getLower() const { return lower; }

    INLINE const T& getUpper() const { return upper; }
};


/// Helper class for iterating over interval using range-based for loop. Cannot be used (and should not be
/// used) in STL algorithms.
template <typename T, typename TStep>
class RangeIterator : public Object {
private:
    T value;
    TStep step;

public:
    RangeIterator(const T& value, TStep step)
        : value(value)
        , step(std::forward<TStep>(step)) {}

    INLINE RangeIterator& operator++() {
        value += step;
        return *this;
    }

    INLINE T& operator*() { return value; }

    INLINE const T& operator*() const { return value; }

    bool operator!=(const RangeIterator& other) {
        // hack: this is actually used as < operator in range-based for loops
        return value < other.value;
    }
};

template <typename T, typename TStep>
class RangeAdapter : public Object {
private:
    Range<T> range;
    TStep step; // can be l-value ref, allowing variable step

public:
    RangeAdapter(const Range<T>& range, TStep&& step)
        : range(range)
        , step(std::forward<TStep>(step)) {}

    INLINE RangeIterator<T, TStep> begin() { return RangeIterator<T, TStep>(range.getLower(), step); }

    INLINE RangeIterator<T, TStep> end() { return RangeIterator<T, TStep>(range.getUpper(), step); }
};

template <typename T, typename TStep>
RangeAdapter<T, TStep> rangeAdapter(const Range<T>& range, TStep&& step) {
    return RangeAdapter<T, TStep>(range, std::forward<TStep>(step));
}


NAMESPACE_SPH_END
