#pragma once

/// Routines for working with intervals.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/Math.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

/// Special value that can hold positive or negative infinity.
class Extended {
private:
    enum class Finiteness { FINITE, PLUS_INF, MINUS_INF };

    Float value;
    Finiteness finiteness = Finiteness::FINITE;

public:
    Extended() = default;

    /// Construcs a finite value.
    INLINE constexpr Extended(const Float value)
        : value(value)
        , finiteness(Finiteness::FINITE) {}

    struct NegativeInfinityTag {};

    struct PositiveInfinityTag {};

    /// Value representing positive infinity. Use unary minus to get negative infinity.
    static constexpr Extended infinity() { return PositiveInfinityTag(); }

    /// Constructs a positive infinity.
    INLINE constexpr Extended(PositiveInfinityTag)
        : value(0._f)
        , finiteness(Finiteness::PLUS_INF) {}

    /// Constructs a negative infinity.
    INLINE constexpr Extended(NegativeInfinityTag)
        : value(0._f)
        , finiteness(Finiteness::MINUS_INF) {}

    /// Checks whether extended value is finite.
    INLINE constexpr bool isFinite() const { return finiteness == Finiteness::FINITE; }

    /// Implicit conversion to value Float, checks that the stored value is finite by assert.
    INLINE constexpr explicit operator Float() const {
        ASSERT(isFinite());
        return value;
    }

    /// Implicit conversion to reference, checks that the stored value is finite by assert.
    INLINE explicit operator Float&() {
        ASSERT(isFinite());
        return value;
    }

    INLINE constexpr Float get() const {
        ASSERT(isFinite());
        return value;
    }

    INLINE Float& get() {
        ASSERT(isFinite());
        return value;
    }

    /// Comparison operator between two extended values. Returns true if both value are finite and equal, or
    /// they are both same Float of infinity.
    INLINE constexpr bool operator==(const Extended& other) const {
        if (isFinite() && other.isFinite()) {
            return value == other.value;
        }
        return finiteness == other.finiteness;
    }

    INLINE constexpr bool operator!=(const Extended& other) const { return !(*this == other); }

    INLINE constexpr bool operator>(const Extended& other) const {
        switch (finiteness) {
        case Finiteness::MINUS_INF:
            return false;
        case Finiteness::PLUS_INF:
            return other.finiteness != Finiteness::PLUS_INF;
        default:
            return other.finiteness == Finiteness::MINUS_INF || (other.isFinite() && value > other.value);
        }
    }

    INLINE constexpr bool operator<(const Extended& other) const {
        switch (finiteness) {
        case Finiteness::PLUS_INF:
            return false;
        case Finiteness::MINUS_INF:
            return other.finiteness != Finiteness::MINUS_INF;
        default:
            return other.finiteness == Finiteness::PLUS_INF || (other.isFinite() && value < other.value);
        }
    }

    INLINE constexpr bool operator>=(const Extended& other) const {
        switch (finiteness) {
        case Finiteness::MINUS_INF:
            return other.finiteness == Finiteness::MINUS_INF;
        case Finiteness::PLUS_INF:
            return true;
        default:
            return other.finiteness == Finiteness::MINUS_INF || (other.isFinite() && value >= other.value);
        }
    }

    INLINE constexpr bool operator<=(const Extended& other) const {
        switch (finiteness) {
        case Finiteness::PLUS_INF:
            return other.finiteness == Finiteness::PLUS_INF;
        case Finiteness::MINUS_INF:
            return true;
        default:
            return other.finiteness == Finiteness::PLUS_INF || (other.isFinite() && value <= other.value);
        }
    }

    INLINE constexpr Extended operator-(const Extended& other) const {
        ASSERT(isFinite() || finiteness != other.finiteness); // not infty-infty, undefined behavior
        if (isFinite() && other.isFinite()) {
            return value - other.value;
        }
        // always returns +-infinite
        if (*this > other) {
            return PositiveInfinityTag();
        } else {
            return NegativeInfinityTag();
        }
    }

    INLINE constexpr int sign() const {
        if (isFinite()) {
            return value > 0;
        } else {
            return finiteness == Finiteness::PLUS_INF;
        }
    }

    INLINE constexpr Extended operator*(const Extended& other) const {
        ASSERT((*this != 0._f || other.isFinite()) && (isFinite() || other != 0._f));
        if (isFinite() && other.isFinite()) {
            return value * other.value;
        }
        const int product = sign() * other.sign();
        if (product > 0) {
            return PositiveInfinityTag();
        } else {
            return NegativeInfinityTag();
        }
    }

    INLINE constexpr Extended operator-() const {
        switch (finiteness) {
        case Finiteness::FINITE:
            return -value;
        case Finiteness::PLUS_INF:
            return NegativeInfinityTag();
        default:
            // negative infinity
            return PositiveInfinityTag();
        }
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Extended& ex) {
        switch (ex.finiteness) {
        case Finiteness::FINITE:
            stream << ex.get();
            break;
        case Finiteness::PLUS_INF:
            stream << "+infinity";
            break;
        case Finiteness::MINUS_INF:
            stream << "-infinity";
            break;
        }
        return stream;
    }
};

/// Object defining 1D interval. Can also represent one sided [x, infty] or [-infty, x], or even "zero" sided
/// [-infty, infty] intervals.
class Range {
private:
    Extended minBound;
    Extended maxBound;

public:
    /// Default construction of an empty interval. Any contains() call will return false, extending the
    /// interval will result in zero-size interval containing the inserted value.
    INLINE Range()
        : minBound(Extended::infinity())
        , maxBound(-Extended::infinity()) {}

    /// Constructs the interval given its lower and upper bound. You can use Extended::infinity() to create
    /// unbounded interval.
    INLINE Range(const Extended& lower, const Extended& upper)
        : minBound(lower)
        , maxBound(upper) {
        ASSERT(lower <= upper);
    }

    INLINE Range(const Range& other)
        : minBound(other.minBound)
        , maxBound(other.maxBound) {}

    /// Extends the interval to contain given value. If the value is already inside the interval, nothing
    /// changes.
    INLINE void extend(const Extended& value) {
        minBound = Math::min(minBound, Extended(value));
        maxBound = Math::max(maxBound, Extended(value));
    }

    /// Checks whether value is inside the interval.
    INLINE bool contains(const Extended& value) const { return minBound <= value && value <= maxBound; }

    /// Clamps the given value by the interval.
    INLINE Float clamp(const Float& value) const {
        ASSERT(minBound <= maxBound);
        const Extended result = Math::max(minBound, Math::min(Extended(value), maxBound));
        ASSERT(result.isFinite());
        return result.get();
    }

    /// Returns lower bound of the interval.
    INLINE const Extended& lower() const { return minBound; }

    /// Returns upper bound of the interval.
    INLINE const Extended& upper() const { return maxBound; }

    /// Returns the size of the interval.
    INLINE Extended size() const { return maxBound - minBound; }

    /// Comparison operator; true if and only if both bounds are equal.
    INLINE bool operator==(const Range& other) const {
        return minBound == other.minBound && maxBound == other.maxBound;
    }

    /// Negation of comparison operator
    INLINE bool operator!=(const Range& other) const { return !(*this == other); }

    static Range unbounded() { return Range(-Extended::infinity(), Extended::infinity()); }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Range& range) {
        stream << range.lower() << " " << range.upper();
        return stream;
    }
};


namespace Math {
    /// Overload of clamp method using range instead of lower and upper bound as values.
    /// Can be used by other Floats by specializing the method
    template <typename T>
    INLINE T clamp(const T& v, const Range& range);

    template <>
    INLINE Float clamp(const Float& v, const Range& range) {
        return range.clamp(v);
    }
    template <>
    INLINE Size clamp(const Size& v, const Range& range) {
        return range.clamp(v);
    }
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

    INLINE Float& operator*() { return value; }

    INLINE const Float& operator*() const { return value; }

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

    INLINE RangeIterator<TStep> begin() { return RangeIterator<TStep>(range.lower().get(), step); }

    INLINE RangeIterator<TStep> end() { return RangeIterator<TStep>(range.upper().get(), step); }
};

template <typename TStep>
RangeAdapter<TStep> rangeAdapter(const Range& range, TStep&& step) {
    return RangeAdapter<TStep>(range, std::forward<TStep>(step));
}

NAMESPACE_SPH_END
