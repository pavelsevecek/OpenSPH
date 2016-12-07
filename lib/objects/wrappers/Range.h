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
    Optional<Float> lower;
    Optional<Float> upper;

public:
    /// Default construction of an empty interval. Any contains() call will return false, extending the
    /// interval will result in zero-size interval containing the inserted value.
    INLINE Range()
        : lower(INFTY)
        , upper(-INFTY) {}

    /// Constructs the interval given its lower and upper bound. You can use NOTHING to create unbounded
    /// interval.
    INLINE Range(const Optional<Float>& lower, const Optional<Float>& upper)
        : lower(lower)
        , upper(upper) {
        ASSERT(!lower || !upper || lower.get() <= upper.get());
    }

    /// Extends the interval to contain given value. If the value is already inside the interval, nothing
    /// changes.
    INLINE void extend(const Float value) {
        if (lower) {
            lower = Math::min(lower.get(), value);
        }
        if (upper) {
            upper = Math::max(upper.get(), value);
        }
    }

    /// Checks whether value is inside the interval.
    INLINE bool contains(const Float value) const {
        return (!lower || lower.get() <= value) && (!upper || value <= upper.get());
    }

    /// Clamps the given value by the interval.
    INLINE Float clamp(const Float value) const {
        ASSERT(!lower || !upper || lower.get() <= upper.get());
        const Float rightBounded = upper ? Math::min(value, upper.get()) : value;
        return lower ? Math::max(lower.get(), rightBounded) : rightBounded;
    }

    /// Returns lower bound of the interval. Asserts if the bound does not exist.
    INLINE const Float& getLower() const {
        ASSERT(lower);
        return lower.get();
    }

    /// Returns upper bound of the interval. Asserts if the bound does not exist.
    INLINE const Float& getUpper() const {
        ASSERT(upper);
        return upper.get();
    }

    /// Comparison operator; true if and only if both bounds are equal.
    INLINE bool operator==(const Range& other) const {
        // note that NOTHING == NOTHING returns false!
        return ((!upper && !other.upper) || upper == other.upper) &&
               ((!lower && !other.lower) || lower == other.lower);
    }

    /// Negation of comparison operator
    INLINE bool operator!=(const Range& other) const {
        return !(*this == other);
    }

    /// Output to stream
    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const Range& range) {
        stream << (range.lower ? std::to_string(range.lower.get()) : std::string("-infinity")) << " "
               << (range.upper ? std::to_string(range.upper.get()) : std::string("infinity"));
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

    INLINE RangeIterator<TStep> begin() { return RangeIterator<TStep>(range.getLower(), step); }

    INLINE RangeIterator<TStep> end() { return RangeIterator<TStep>(range.getUpper(), step); }
};

template <typename TStep>
RangeAdapter<TStep> rangeAdapter(const Range& range, TStep&& step) {
    return RangeAdapter<TStep>(range, std::forward<TStep>(step));
}

NAMESPACE_SPH_END
