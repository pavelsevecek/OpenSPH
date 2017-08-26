#pragma once

/// \file Interval.h
/// \brief Object representing interval of real values
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Globals.h"
#include "math/Math.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN

/// Object defining 1D interval. Can also represent one sided [x, infty] or [-infty, x], or even unbounded
/// [-infty, infty] intervals.
class Interval {
private:
    Float minBound;
    Float maxBound;

public:
    /// Default construction of an empty interval. Any contains() call will return false, extending the
    /// interval will result in zero-size interval containing the inserted value.
    INLINE Interval()
        : minBound(INFTY)
        , maxBound(-INFTY) {}

    /// Constructs the interval given its lower and upper bound. You can use INFTY and -INFTY to create
    /// one-sided or unbounded intervals.
    INLINE Interval(const Float& lower, const Float& upper)
        : minBound(lower)
        , maxBound(upper) {
        ASSERT(lower <= upper);
    }

    INLINE Interval(const Interval& other)
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

    /// Returns the center of the interval
    INLINE Float center() const {
        return 0.5_f * (minBound + maxBound);
    }

    /// Returns the size of the interval.
    INLINE Float size() const {
        return maxBound - minBound;
    }

    /// Comparison operator; true if and only if both bounds are equal.
    INLINE bool operator==(const Interval& other) const {
        return minBound == other.minBound && maxBound == other.maxBound;
    }

    /// Negation of comparison operator
    INLINE bool operator!=(const Interval& other) const {
        return !(*this == other);
    }

    INLINE bool isEmpty() const {
        return minBound > maxBound;
    }

    static Interval unbounded() {
        return Interval(-INFTY, INFTY);
    }

    friend std::ostream& operator<<(std::ostream& stream, const Interval& range);
};


/// Overload of clamp method using range instead of lower and upper bound as values.
/// Can be used by other Floats by specializing the method
template <typename T>
INLINE T clamp(const T& v, const Interval& range) {
    return range.clamp(v);
}


/// Returns clamped values and the value of derivative. The function is specialized for floats,
/// returning zero derivative if the value is outside range.
/// \todo clamp derivatives for all types? So far we don't clamp non-scalar quantities anyway ...
template <typename T>
INLINE Pair<T> clampWithDerivative(const T&, const T&, const Interval&) {
    NOT_IMPLEMENTED;
}

template <>
INLINE Pair<Float> clampWithDerivative<Float>(const Float& v, const Float& dv, const Interval& range) {
    const bool zeroDeriv = (v >= range.upper() && dv > 0._f) || (v <= range.lower() && dv < 0._f);
    return { clamp(v, range), zeroDeriv ? 0._f : dv };
}

NAMESPACE_SPH_END
