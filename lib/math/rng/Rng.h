#pragma once

/// Random number generators and deterministic sequences with low discrepancy for Monte Carlo and
/// quasi-Monte Carlo computations
/// Pavel Sevecek 2015
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Bounds.h"
#include "objects/containers/Array.h"
#include <random>

NAMESPACE_SPH_BEGIN

/// Random number generator with uniform distribution.
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


/// Random number generator used in code SPH5 of Benz & Asphaug (1994), reimplemented for reproducibility of
/// results.
class BenzAsphaugRng : public Noncopyable {
private:
    const int seed;
    const int im1    = 2147483563;
    const int im2    = 2147483399;
    const Float am   = 1._f / im1;
    const int imm1   = im1 - 1;
    const int ia1    = 40014;
    const int ia2    = 40692;
    const int iq1    = 53668;
    const int iq2    = 52774;
    const int ir1    = 12211;
    const int ir2    = 3791;
    const Float eps  = 1.2e-7_f;
    const Float rnmx = 1._f - eps;

    static constexpr int ntab = 32;
    StaticArray<int, ntab> iv;
    int iy    = 0;
    int idum2 = 123456789;
    int idum;

public:
    BenzAsphaugRng(const int seed)
        : seed(seed) {
        idum = seed;
        std::fill(iv.begin(), iv.end(), 0);
    }

    BenzAsphaugRng(BenzAsphaugRng&& other)
        : seed(other.seed)
        , iv(std::move(other.iv))
        , iy(other.iy)
        , idum2(other.idum2)
        , idum(other.idum) {}

    INLINE Float operator()(const int UNUSED(s) = 0) {
        const int ndiv     = 1 + imm1 / ntab;

        if (idum < 0) {
            idum  = Math::max(-idum, 1);
            idum2 = idum;
            for (int j = ntab + 8; j >= 1; --j) {
                const int k = idum / iq1;
                idum        = ia1 * (idum - k * iq1) - k * ir1;
                if (idum < 0) {
                    idum = idum + im1;
                }
                if (j < ntab) {
                    iv[j-1] = idum;
                }
            }
            iy = iv[0];
        }
        int k = idum / iq1;
        idum  = ia1 * (idum - k * iq1) - k * ir1;
        if (idum < 0) {
            idum = idum + im1;
        }
        k     = idum2 / iq2;
        idum2 = ia2 * (idum2 - k * iq2) - k * ir2;
        if (idum2 < 0) {
            idum2 = idum2 + im2;
        }
        const int j = iy / ndiv;
        iy          = iv[j] - idum2;
        iv[j]       = idum;
        if (iy < 1) {
            iy = iy + imm1;
        }
        return Math::min(am * iy, rnmx);
    }
};

/// Quasi-random number generator.
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
