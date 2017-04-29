#pragma once

/// Computing minimum, maximum and average value of floats.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/wrappers/Range.h"

NAMESPACE_SPH_BEGIN

/// Object computing generalized means from a set of floating-point values, namely minimum, maximum and
/// arithmetic mean.
class Means {
private:
    double sum = 0.; // using double to limit round-off errors in summing
    Size weight = 0;
    Range rangeValue;

public:
    Means() = default;

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

    INLINE Float average() const {
        return Float(sum / weight);
    }

    INLINE Float min() const {
        return rangeValue.lower();
    }

    INLINE Float max() const {
        return rangeValue.upper();
    }

    INLINE Range range() const {
        return rangeValue;
    }

    INLINE Size count() const {
        return weight;
    }

    friend std::ostream& operator<<(std::ostream& stream, const Means& stats) {
        stream << "average = " << stats.average() << "  (min = " << stats.min() << ", max = " << stats.max()
               << ")";
        return stream;
    }
};

NAMESPACE_SPH_END
