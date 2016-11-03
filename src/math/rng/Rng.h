#pragma once

/// Random number generators and deterministic sequences with low discrepancy for Monte Carlo and
/// quasi-Monte Carlo computations
/// Pavel Sevecek 2015
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Bounds.h"
#include "objects/containers/Array.h"
#include <random>

NAMESPACE_SPH_BEGIN

/// A random number generator with uniform distribution.
/// Can be used to generate random quantities with units.
class UniformRng : public Noncopyable {
private:
    std::mt19937_64 mt;
    std::uniform_real_distribution<Float> dist;

public:
    UniformRng(const int seed = 1234) { mt = std::mt19937_64(seed); }

    UniformRng(UniformRng&& other)
        : mt(other.mt)
        , dist(other.dist) {}

    Float operator()(const int UNUSED(s) = 0) { return dist(mt); }
};

/// A quasi-random number generator.
/// Can be used to generate random quantities with units.
class HaltonQrng : public Noncopyable {
protected:
    static const int dimension = 6;
    StaticArray<int, dimension> primes; // TODO: extend in case more is necessary
    StaticArray<int, dimension> c;

    Float radicalInverse(const int base, int i) {
        Float p       = Float(base);
        int j         = i;
        Float inverse = 0._f;
        Float a;
        while (j > 0) {
            a = Float(j % base);
            inverse += a / p;
            j = int(j / base);
            p *= base;
        }
        return Float(inverse);
    }

public:
    HaltonQrng()
        : primes{ 2, 3, 5, 7, 11, 13 } {
        std::fill(c.begin(), c.end(), 0);
    }

    HaltonQrng(HaltonQrng&& other)
        : primes(std::move(other.primes))
        , c(std::move(other.c)) {}

    Float operator()(const int s) {
        ASSERT(s < 6);
        return radicalInverse(primes[s], c[s]++);
    }
};


NAMESPACE_SPH_END
