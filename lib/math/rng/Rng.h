#pragma once

/// \file Rng.h
/// \brief Random number generators
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/Array.h"
#include "objects/geometry/Box.h"
#include "objects/wrappers/AutoPtr.h"
#include <random>

NAMESPACE_SPH_BEGIN

/// \brief Random number generator with uniform distribution.
class UniformRng : public Noncopyable {
private:
    std::mt19937_64 mt;

    /// \note While std::mt19937_64 will always generate the same sequence of numbers (given the same seed, of
    /// cource), the std::uniform_real_distribution might produce different result for different compilers, or
    /// even for different versions of the compiler.
    std::uniform_real_distribution<Float> dist;

public:
    UniformRng(const int seed = 1234) {
        mt = std::mt19937_64(seed);
    }

    UniformRng(UniformRng&& other)
        : mt(other.mt)
        , dist(other.dist) {}

    Float operator()(const int UNUSED(s) = 0) {
        return dist(mt);
    }
};


/// \brief Random number generator used in code SPH5 of Benz & Asphaug (1994).
///
/// Reimplemented for reproducibility of results.
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

/// \brief Quasi-random number generator.
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


/// \brief Polymorphic holder allowing to store any RNG (type erasure).
class IRng : public Polymorphic {
public:
    /// Generates a random number.
    virtual Float operator()(const int s = 0) = 0;
};

template <typename TRng>
class RngWrapper : public IRng {
private:
    TRng rng;

public:
    template <typename... TArgs>
    RngWrapper(TArgs&&... args)
        : rng(std::forward<TArgs>(args)...) {}

    virtual Float operator()(const int s) override {
        return rng(s);
    }
};

template <typename TRng, typename... TArgs>
AutoPtr<RngWrapper<TRng>> makeRng(TArgs&&... args) {
    return makeAuto<RngWrapper<TRng>>(std::forward<TArgs>(args)...);
}

/// \brief Generates a random number from normal distribution, using Box-Muller algorithm.
///
/// Could be optimized (it discards the second independent value), but it's the algorith easiest to implement.
template <typename TRng>
INLINE Float sampleNormalDistribution(TRng& rng, const Float mu, const Float sigma) {
    static const Float epsilon = std::numeric_limits<Float>::min();

    Float u1, u2;
    do {
        u1 = rng();
        u2 = rng();
    } while (u1 <= epsilon);

    const Float z1 = sqrt(-2._f * log(u1)) * cos(2._f * PI * u2);
    ASSERT(isReal(z1));
    return z1 * sigma + mu;
}

/// \brief Generates a random number from exponential distribution.
///
/// Uses inverse transform sampling.
template <typename TRng>
INLINE Float sampleExponentialDistribution(TRng& rng, const Float lambda) {
    static const Float epsilon = std::numeric_limits<Float>::min();

    Float u;
    do {
        u = rng();
    } while (u <= epsilon);
    return -log(rng()) / lambda;
}

/// \brief Generates a random integer from Poisson distribution.
///
/// Uses the Knuth's algorithm.
template <typename TRng>
INLINE Size samplePoissonDistribution(TRng& rng, const Float lambda) {
    const Float l = exp(-lambda);
    Size k = 0;
    Float p = 1;
    do {
        k = k + 1;
        const Float u = rng();
        p = p * u;
    } while (p > l);
    return k - 1;
}

NAMESPACE_SPH_END
