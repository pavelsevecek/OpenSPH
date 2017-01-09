#pragma once

#include "core/Globals.h"
#include "objects/Object.h"

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

    /// Explicit conversion to value Float, checks that the stored value is finite by assert.
    INLINE constexpr explicit operator Float() const {
        ASSERT(isFinite());
        return value;
    }

    /// Explicit conversion to reference, checks that the stored value is finite by assert.
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

    INLINE constexpr Extended operator+(const Extended& other) const {
        // not infty+(-infty), undefined behavior
        ASSERT(isFinite() || other.isFinite() || finiteness == other.finiteness);
        if (isFinite() && other.isFinite()) {
            return value + other.value;
        }
        // always returns +-infinite
        if (finiteness == Finiteness::PLUS_INF || other.finiteness == Finiteness::PLUS_INF) {
            return PositiveInfinityTag();
        } else {
            return NegativeInfinityTag();
        }
    }

    INLINE constexpr Extended operator-(const Extended& other) const {
        // not infty-infty, undefined behavior
        ASSERT(isFinite() || other.isFinite() || finiteness != other.finiteness);
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
            return value > 0 ? 1 : -1;
        } else {
            return (finiteness == Finiteness::PLUS_INF) ? 1 : -1;
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


    INLINE constexpr Extended operator/(const Extended& other) const {
        ASSERT((*this != 0._f || other != 0._f) && (isFinite() || other.isFinite()));
        if (isFinite() && other.isFinite()) {
            return value / other.value;
        }
        const int product = sign() * other.sign();
        if (isFinite()) {
            // x / infty
            return 0._f;
        } else {
            // infty / x;
            if (product > 0) {
                return PositiveInfinityTag();
            } else {
                return NegativeInfinityTag();
            }
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

NAMESPACE_SPH_END
