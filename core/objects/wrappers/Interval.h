#pragma once

/// \file Interval.h
/// \brief Object representing interval of real values
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Globals.h"
#include "math/MathUtils.h"
#include "objects/containers/StaticArray.h"
#include <iomanip>

NAMESPACE_SPH_BEGIN

/// \brief Object representing a 1D interval of real numbers.
///
/// Can also represent one sided [x, infty] or [-infty, x], or even unbounded [-infty, infty] intervals.
class Interval {
private:
    Float minBound;
    Float maxBound;

public:
    /// \brief Default construction of an empty interval.
    ///
    /// Any contains() call will return false, extending the interval will result in zero-size interval
    /// containing the inserted value.
    INLINE Interval()
        : minBound(INFTY)
        , maxBound(-INFTY) {}

    /// \brief Constructs the interval given its lower and upper bound.
    ///
    /// You can use INFTY and -INFTY to create one-sided or unbounded intervals.
    INLINE Interval(const Float& lower, const Float& upper)
        : minBound(lower)
        , maxBound(upper) {}

    /// \brief Extends the interval to contain given value.
    ///
    /// If the value is already inside the interval, nothing changes.
    INLINE void extend(const Float& value) {
        minBound = min(minBound, value);
        maxBound = max(maxBound, value);
    }

    /// \brief Extends the interval to contain another interval.
    ///
    /// If the other interval is already inside this interval, nothing changes.
    INLINE void extend(const Interval& other) {
        minBound = min(minBound, other.minBound);
        maxBound = max(maxBound, other.maxBound);
    }

    /// \brief Checks whether value is inside the interval.
    INLINE bool contains(const Float& value) const {
        return minBound <= value && value <= maxBound;
    }

    /// \brief Checks if two intervals have non-empty intersection.
    INLINE bool intersects(const Interval& other) const {
        return !intersection(other).empty();
    }

    /// \brief Computes the intersection with another interval.
    INLINE Interval intersection(const Interval& other) const {
        Interval is;
        is.minBound = max(minBound, other.minBound);
        is.maxBound = min(maxBound, other.maxBound);
        return is;
    }

    /// \brief Clamps the given value by the interval.
    INLINE Float clamp(const Float& value) const {
        SPH_ASSERT(minBound <= maxBound);
        return max(minBound, min(value, maxBound));
    }

    /// \brief Returns lower bound of the interval.
    INLINE Float lower() const {
        return minBound;
    }

    /// \brief Returns upper bound of the interval.
    INLINE Float upper() const {
        return maxBound;
    }

    /// \brief Returns the center of the interval
    INLINE Float center() const {
        return 0.5_f * (minBound + maxBound);
    }

    /// \brief Returns the size of the interval.
    INLINE Float size() const {
        return maxBound - minBound;
    }

    /// \brief Comparison operator; true if and only if both bounds are equal.
    INLINE bool operator==(const Interval& other) const {
        return minBound == other.minBound && maxBound == other.maxBound;
    }

    /// \brief Negation of comparison operator
    INLINE bool operator!=(const Interval& other) const {
        return !(*this == other);
    }

    /// \brief Returns true if the interval is empty (default constructed).
    INLINE bool empty() const {
        return minBound > maxBound;
    }

    /// \brief Returns an unbounded (infinite) interval
    static Interval unbounded() {
        return Interval(-INFTY, INFTY);
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Interval& range) {
        // wrapper over float printing "infinity/-infinity" instead of value itself
        struct IntervalPrinter {
            Float value;

            void print(TStream& stream) {
                stream << std::setw(20);
                if (value == INFTY) {
                    stream << L"infinity";
                } else if (value == -INFTY) {
                    stream << L"-infinity";
                } else {
                    stream << value;
                }
            }
        };
        IntervalPrinter{ range.lower() }.print(stream);
        IntervalPrinter{ range.upper() }.print(stream);
        return stream;
    }
};

/// Overload of clamp method using range instead of lower and upper bound as values.
/// Can be used by other Floats by specializing the method
template <typename T>
INLINE T clamp(const T& v, const Interval& range) {
    return T(range.clamp(v));
}

/// \brief Returns clamped values and the value of derivative.
///
/// The function is specialized for floats, returning zero derivative if the value is outside range.
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
