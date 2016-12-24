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
    const int im1 = 2147483563;
    const int im2 = 2147483399;
    const Float am = 1._f / im1;
    const int imm1 = im1 - 1;
    const int ia1 = 40014;
    const int ia2 = 40692;
    const int iq1 = 53668;
    const int iq2 = 52774;
    const int ir1 = 12211;
    const int ir2 = 3791;
    const Float eps = 1.2e-7_f;
    const Float rnmx = 1._f - eps;

    static constexpr int ntab = 32;
    StaticArray<int, ntab> iv;
    int iy = 0;
    int idum2 = 123456789;
    int idum;

public:
    BenzAsphaugRng(const int seed);

    BenzAsphaugRng(BenzAsphaugRng&& other);

    Float operator()(const int s = 0);
};

/// Quasi-random number generator.
class HaltonQrng : public Noncopyable {
protected:
    static const int dimension = 6;
    StaticArray<int, dimension> primes; /// \todo extend in case more is necessary
    StaticArray<int, dimension> c;

public:
    HaltonQrng();

    HaltonQrng(HaltonQrng&& other);

    Float operator()(const int s);
};


NAMESPACE_SPH_END
