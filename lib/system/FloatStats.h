#pragma once

#include "objects/wrappers/Range.h"

NAMESPACE_SPH_BEGIN

/// Simple object that gathers float values and computes mean, min and max.
class FloatStats {
private:
    double sum = 0.; // using double to limit round-off errors in summing
    Size weight = 0;
    Range rangeValue;

public:
    FloatStats() = default;

    /// Adds a value into the set from which the stats (min, max, average) are computed.
    INLINE void accumulate(const Float value) {
        sum += value;
        weight++;
        rangeValue.extend(value);
    }

    /// Removes all values from the set.
    INLINE void reset() {
        sum = 0.;
        weight = 0;
        rangeValue = Range();
    }

    INLINE Float average() const { return Float(sum / weight); }

    INLINE Extended min() const { return rangeValue.lower(); }

    INLINE Extended max() const { return rangeValue.upper(); }

    INLINE Range range() const { return rangeValue; }

    INLINE Size count() const { return weight; }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const FloatStats& stats) {
        stream << "average = " << stats.average() << "  (min = " << stats.min() << ", max = " << stats.max()
               << ")";
    }
};

NAMESPACE_SPH_END
