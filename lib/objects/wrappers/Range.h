#pragma once

/// Routines for working with intervals.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "math/Math.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

/// Object defining 1D interval. Can also represent one sided [x, infty] or [-infty, x], or even "zero" sided
/// [-infty, infty] intervals.
class Range : public Object {
private:
    Optional<Float> minBound;
    Optional<Float> maxBound;

public:
    /// Default construction of an empty interval. Any contains() call will return false, extending the
    /// interval will result in zero-size interval containing the inserted value.
    INLINE Range()
        : minBound(INFTY)
        , maxBound(-INFTY) {}

    /// Constructs the interval given its lower and upper bound. You can use NOTHING to create unbounded
    /// interval.
    INLINE Range(const Optional<Float>& lower, const Optional<Float>& upper)
        : minBound(lower)
        , maxBound(upper) {
        ASSERT(!lower || !upper || lower.get() <= upper.get());
    }

    /// Extends the interval to contain given value. If the value is already inside the interval, nothing
    /// changes.
    INLINE void extend(const Float value) {
        if (minBound) {
            minBound = Math::min(minBound.get(), value);
        }
        if (maxBound) {
            maxBound = Math::max(maxBound.get(), value);
        }
    }

    /// Checks whether value is inside the interval.
    INLINE bool contains(const Float value) const {
        return (!minBound || minBound.get() <= value) && (!maxBound || value <= maxBound.get());
    }

    /// Clamps the given value by the interval.
    INLINE Float clamp(const Float value) const {
        ASSERT(!minBound || !maxBound || minBound.get() <= maxBound.get());
        const Float rightBounded = maxBound ? Math::min(value, maxBound.get()) : value;
        return minBound ? Math::max(minBound.get(), rightBounded) : rightBounded;
    }

    /// Returns lower bound of the interval. Asserts if the bound does not exist.
    INLINE Float lower() const {
        ASSERT(minBound);
        return minBound.get();
    }

    /// Returns upper bound of the interval. Asserts if the bound does not exist.
    INLINE Float upper() const {
        ASSERT(maxBound);
        return maxBound.get();
    }

    ///Returns the size of the interval. If the interval is unbounded, returns infinity
    INLINE Float size() const {
        if (!minBound || !maxBound) {
            return std::numeric_limits<Float>::infinity();
        }
        return maxBound.get() - minBound.get();
    }

    /// Comparison operator; true if and only if both bounds are equal.
    INLINE bool operator==(const Range& other) const {
        // note that NOTHING == NOTHING returns false!
        return ((!maxBound && !other.maxBound) || maxBound == other.maxBound) &&
               ((!minBound && !other.minBound) || minBound == other.minBound);
    }

    /// Negation of comparison operator
    INLINE bool operator!=(const Range& other) const {
        return !(*this == other);
    }

    /// Output to stream
    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Range& range) {
        stream << (range.minBound ? std::to_string(range.minBound.get()) : std::string("-infinity")) << " "
               << (range.maxBound ? std::to_string(range.maxBound.get()) : std::string("infinity"));
        return stream;
    }

    static Range unbounded() {
        return Range(NOTHING, NOTHING);
    }
};


namespace Math {
    /// Overload of clamp method using range instead of lower and upper bound as values.
    /// Can be used by other types by specializing the method
    template <typename T>
    INLINE T clamp(const T& v, const Range& range);

    template <>
    INLINE Float clamp(const Float& v, const Range& range) {
        return range.clamp(v);
    }
    template <>
    INLINE int clamp(const int& v, const Range& range) {
        return range.clamp(v);
    }
}

/// Helper class for iterating over interval using range-based for loop. Cannot be used (and should not be
/// used) in STL algorithms.
template <typename TStep>
class RangeIterator : public Object {
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
class RangeAdapter : public Object {
private:
    Range range;
    TStep step; // can be l-value ref, allowing variable step

public:
    RangeAdapter(const Range& range, TStep&& step)
        : range(range)
        , step(std::forward<TStep>(step)) {}

    INLINE RangeIterator<TStep> begin() { return RangeIterator<TStep>(range.lower(), step); }

    INLINE RangeIterator<TStep> end() { return RangeIterator<TStep>(range.upper(), step); }
};

template <typename TStep>
RangeAdapter<TStep> rangeAdapter(const Range& range, TStep&& step) {
    return RangeAdapter<TStep>(range, std::forward<TStep>(step));
}

NAMESPACE_SPH_END
