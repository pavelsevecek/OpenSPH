#include "math/rng/Rng.h"

NAMESPACE_SPH_BEGIN

BenzAsphaugRng::BenzAsphaugRng(const int seed)
    : seed(seed) {
    idum = seed;
    std::fill(iv.begin(), iv.end(), 0);
}

BenzAsphaugRng::BenzAsphaugRng(BenzAsphaugRng&& other)
    : seed(other.seed)
    , iv(std::move(other.iv))
    , iy(other.iy)
    , idum2(other.idum2)
    , idum(other.idum) {}

Float BenzAsphaugRng::operator()(const int UNUSED(s)) {
    const int ndiv = 1 + imm1 / ntab;

    if (idum < 0) {
        idum = Math::max(-idum, 1);
        idum2 = idum;
        for (int j = ntab + 8; j >= 1; --j) {
            const int k = idum / iq1;
            idum = ia1 * (idum - k * iq1) - k * ir1;
            if (idum < 0) {
                idum = idum + im1;
            }
            if (j < ntab) {
                iv[j - 1] = idum;
            }
        }
        iy = iv[0];
    }
    int k = idum / iq1;
    idum = ia1 * (idum - k * iq1) - k * ir1;
    if (idum < 0) {
        idum = idum + im1;
    }
    k = idum2 / iq2;
    idum2 = ia2 * (idum2 - k * iq2) - k * ir2;
    if (idum2 < 0) {
        idum2 = idum2 + im2;
    }
    const int j = iy / ndiv;
    iy = iv[j] - idum2;
    iv[j] = idum;
    if (iy < 1) {
        iy = iy + imm1;
    }
    return Math::min(am * iy, rnmx);
}


INLINE Float radicalInverse(const int base, int i) {
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

HaltonQrng::HaltonQrng()
    : primes{ 2, 3, 5, 7, 11, 13 } {
    std::fill(c.begin(), c.end(), 0);
}

HaltonQrng::HaltonQrng(HaltonQrng&& other)
    : primes(std::move(other.primes))
    , c(std::move(other.c)) {}

Float HaltonQrng::operator()(const int s) {
    ASSERT(s < 6);
    return radicalInverse(primes[s], c[s]++);
}


NAMESPACE_SPH_END
