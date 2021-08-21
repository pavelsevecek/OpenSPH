#pragma once

/// \file Means.h
/// \brief Computing minimum, maximum and mean value of floats.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/geometry/Generic.h"
#include "objects/wrappers/Interval.h"

NAMESPACE_SPH_BEGIN

/// \brief Generalized mean with fixed (compile-time) power.
template <int Power>
class GeneralizedMean {
private:
    double sum = 0.; // using double to limit round-off errors in summing
    Size weight = 0;

public:
    GeneralizedMean() = default;

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
        stream << stats.compute();
        return stream;
    }
};

/// Geometric mean has to be specialized.
template <>
class GeneralizedMean<0> {
private:
    double sum = 1.;
    Size weight = 0;

public:
    GeneralizedMean() = default;

    INLINE void accumulate(const Float value) {
        sum *= value;
        weight++;
    }

    INLINE void reset() {
        sum = 1.;
        weight = 0;
    }

    INLINE Float compute() const {
        return Float(pow(sum, 1. / weight));
    }

    INLINE Size count() const {
        return weight;
    }

    friend std::ostream& operator<<(std::ostream& stream, const GeneralizedMean& stats) {
        stream << stats.compute();
        return stream;
    }
};


/// Aliases
using ArithmeticMean = GeneralizedMean<1>;
using HarmonicMean = GeneralizedMean<-1>;
using GeometricMean = GeneralizedMean<0>;


/// \brief Generalized mean with positive (runtime) power.
///
/// Cannot be used to compute geometric mean. If constructed with non-positive power, it issues an assert.
class PositiveMean {
protected:
    double sum = 0.;
    Size weight = 0;
    Float power;

public:
    PositiveMean(const Float power)
        : power(power) {
        SPH_ASSERT(power > 0._f);
    }

    INLINE void accumulate(const Float value) {
        sum += powFastest(value, power);
        weight++;
    }

    INLINE void accumulate(const PositiveMean& other) {
        SPH_ASSERT(power == other.power); // it only makes sense to sum up same means
        sum += other.sum;
        weight += other.weight;
    }

    INLINE void reset() {
        sum = 0.;
        weight = 0;
    }

    INLINE Float compute() const {
        return Float(powFastest(Float(sum / weight), 1._f / power));
    }

    INLINE Size count() const {
        return weight;
    }

    friend std::ostream& operator<<(std::ostream& stream, const PositiveMean& stats) {
        stream << stats.compute();
        return stream;
    }
};

/// \brief Generalized mean with negative (runtime) power.
///
/// Cannot be used to compute geometric mean. If constructed with non-negative power, it issues an assert.
class NegativeMean : public PositiveMean {
public:
    NegativeMean(const Float power)
        : PositiveMean(-power) {}

    INLINE void accumulate(const Float value) {
        SPH_ASSERT(value > 0._f, value);
        const Float p = pow(value, power);
        if (p == INFTY) {
            weight++; // just increase weight
        } else if (p > 0._f) {
            sum += 1._f / p;
            weight++;
        }
    }

    INLINE void accumulate(const NegativeMean& other) {
        SPH_ASSERT(power == other.power);
        sum += other.sum;
        weight += other.weight;
    }

    INLINE Float compute() const {
        Float avg = Float(sum / weight);
        SPH_ASSERT(isReal(avg), avg, sum, weight);
        Float avgPow = pow(avg, 1._f / power);
        if (avgPow == 0._f) {
            return INFINITY;
        } else {
            return 1._f / avgPow;
        }
    }
};

/// Helper class for statistics, accumulating minimal, maximal and mean value of a set of numbers.
class MinMaxMean {
private:
    Interval minMax;
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
        minMax = Interval();
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

    INLINE Interval range() const {
        return minMax;
    }

    INLINE Size count() const {
        return avg.count();
    }

    template <typename TStream>
    friend TStream& operator<<(TStream& stream, const MinMaxMean& stats) {
        stream << "average = " << stats.mean() << "  (min = " << stats.min() << ", max = " << stats.max()
               << ")";
        return stream;
    }
};

NAMESPACE_SPH_END
