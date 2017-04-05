#pragma once

/// Routines for working with intervals.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/Math.h"
#include "objects/containers/StaticArray.h"
#include <iomanip>

NAMESPACE_SPH_BEGIN

/// Object defining 1D interval. Can also represent one sided [x, infty] or [-infty, x], or even unbounded
/// [-infty, infty] intervals.
class Range {
private:
    Float minBound;
    Float maxBound;

public:
    /// Default construction of an empty interval. Any contains() call will return false, extending the
    /// interval will result in zero-size interval containing the inserted value.
    INLINE Range()
        : minBound(INFTY)
        , maxBound(-INFTY) {}

    /// Constructs the interval given its lower and upper bound. You can use INFTY and -INFTY to create
    /// one-sided or unbounded intervals.
    INLINE Range(const Float& lower, const Float& upper)
        : minBound(lower)
        , maxBound(upper) {
        ASSERT(lower <= upper);
    }

    INLINE Range(const Range& other)
        : minBound(other.minBound)
        , maxBound(other.maxBound) {}

    /// Extends the interval to contain given value. If the value is already inside the interval, nothing
    /// changes.
    INLINE void extend(const Float& value) {
        minBound = min(minBound, value);
        maxBound = max(maxBound, value);
    }

    /// Checks whether value is inside the interval.
    INLINE bool contains(const Float& value) const {
        return minBound <= value && value <= maxBound;
    }

    /// Clamps the given value by the interval.
    INLINE Float clamp(const Float& value) const {
        ASSERT(minBound <= maxBound);
        return max(minBound, min(value, maxBound));
    }

    /// Returns lower bound of the interval.
    INLINE Float lower() const {
        return minBound;
    }

    /// Returns upper bound of the interval.
    INLINE Float upper() const {
        return maxBound;
    }

    /// Returns the size of the interval.
    INLINE Float size() const {
        return maxBound - minBound;
    }

    /// Comparison operator; true if and only if both bounds are equal.
    INLINE bool operator==(const Range& other) const {
        return minBound == other.minBound && maxBound == other.maxBound;
    }

    /// Negation of comparison operator
    INLINE bool operator!=(const Range& other) const {
        return !(*this == other);
    }

    static Range unbounded() {
        return Range(-INFTY, INFTY);
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Range& range) {
        stream << Printer{ range.lower() } << Printer{ range.upper() };
        return stream;
    }

private:
    // wrapper over float printing "infinity/-infinity" instead of value itself
    struct Printer {
        Float value;

        template <typename TStream>
        friend TStream& operator<<(TStream& stream, const Printer w) {
            stream << std::setw(25);
            if (w.value == INFTY) {
                stream << "infinity";
            } else if (w.value == -INFTY) {
                stream << "-infinity";
            } else {
                stream << w.value;
            }
            return stream;
        }
    };
};


/// Overload of clamp method using range instead of lower and upper bound as values.
/// Can be used by other Floats by specializing the method
template <typename T>
INLINE T clamp(const T& v, const Range& range) {
    return range.clamp(v);
}

/// Returns clamped values of object. For components that were clamped by the range, corresponding components
/// in the second parameter are set to zero. Other components are unchanged.
/// The intended use case is for clamping values of time-dependent quantities; the derivatives must also be
/// clamped to avoid instabilities of timestepping algorithm.
template <typename T>
INLINE Pair<T> clampWithDerivative(const T& v, const T& dv, const Range& range) {
    const T lower = less(T(range.lower()), v);
    const T upper = less(v, T(range.upper()));
    /// \todo optimize
    return { clamp(v, range), dv * lower * upper };
}


/// Helper class for iterating over interval using range-based for loop. Cannot be used (and should not be
/// used) in STL algorithms.
template <typename TStep>
class RangeIterator {
private:
    Float value;
    TStep step;

public:
    RangeIterator(const Float value, TStep step)
        : value(value)
        , step(std::forward<TStep>(step)) {}

    INLINE RangeIterator& operator++() {
        value += step;
        return *this;
    }

    INLINE Float& operator*() {
        return value;
    }

    INLINE const Float& operator*() const {
        return value;
    }

    bool operator!=(const RangeIterator& other) {
        // hack: this is actually used as < operator in range-based for loops
        return value < other.value;
    }
};

template <typename TStep>
class RangeAdapter {
private:
    Range range;
    TStep step; // can be l-value ref, allowing variable step

public:
    RangeAdapter(const Range& range, TStep&& step)
        : range(range)
        , step(std::forward<TStep>(step)) {}

    INLINE RangeIterator<TStep> begin() {
        return RangeIterator<TStep>(range.lower(), step);
    }

    INLINE RangeIterator<TStep> end() {
        return RangeIterator<TStep>(range.upper(), step);
    }
};

template <typename TStep>
RangeAdapter<TStep> rangeAdapter(const Range& range, TStep&& step) {
    return RangeAdapter<TStep>(range, std::forward<TStep>(step));
}

NAMESPACE_SPH_END
