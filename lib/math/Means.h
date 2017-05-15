#pragma once

/// \file Means.h
/// \brief Computing minimum, maximum and mean value of floats.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/wrappers/Range.h"

NAMESPACE_SPH_BEGIN

template <int Power>
class GeneralizedMean {
private:
    double sum = 0.; // using double to limit round-off errors in summing
    Size weight = 0;

public:
    GeneralizedMean() = default;

    /// Adds a value into the set from which the stats (min, max, average) are computed.
    INLINE void accumulate(const Float value) {
        sum += pow<Power>(value);
        weight++;
    }

    /// Removes all values from the set.
    INLINE void reset() {
        sum = 0.;
        weight = 0;
    }

    INLINE Float compute() const {
        return Float(pow(sum / weight, 1. / Power));
    }

    INLINE Size count() const {
        return weight;
    }

    friend std::ostream& operator<<(std::ostream& stream, const GeneralizedMean& stats) {
        stream << stats.mean();
        return stream;
    }
};

/// Aliases
using ArithmeticMean = GeneralizedMean<1>;
using HarmonicMean = GeneralizedMean<-1>;


/// Helper class for statistics
class MinMaxMean {
private:
    Range minMax;
    ArithmeticMean avg;

public:
    MinMaxMean() = default;

    INLINE void accumulate(const Float value) {
        avg.accumulate(value);
        minMax.extend(value);
    }

    /// Removes all values from the set.
    INLINE void reset() {
        avg.reset();
        minMax = Range();
    }

    INLINE Float mean() const {
        return avg.compute();
    }

    INLINE Float min() const {
        return minMax.lower();
    }

    INLINE Float max() const {
        return minMax.upper();
    }

    INLINE Range range() const {
        return minMax;
    }

    INLINE Size count() const {
        return avg.count();
    }

    friend std::ostream& operator<<(std::ostream& stream, const MinMaxMean& stats) {
        stream << "average = " << stats.mean() << "  (min = " << stats.min() << ", max = " << stats.max()
               << ")";
        return stream;
    }
};

NAMESPACE_SPH_END
