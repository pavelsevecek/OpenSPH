#include "math/rng/Rng.h"
#include "catch.hpp"
#include "math/Math.h"

using namespace Sph;

template <typename TRng>
void testRng(TRng rng) {
    double minValue = 1000., maxValue = -1000.;
    double sum    = 0.;
    double sumSqr = 0.;
    double sumCb  = 0.;
    int N         = 1000000;
    double norm   = 1. / double(N);
    for (int i = 0; i < N; ++i) {
        double value = rng(0);
        minValue     = min(minValue, value);
        maxValue     = max(maxValue, value);
        sum += value;
        sumSqr += sqr(value);
        sumCb += pow<3>((Float)value);
    }
    REQUIRE(minValue >= 0.);
    REQUIRE(minValue <= 1.e-5);
    REQUIRE(maxValue <= 1.);
    REQUIRE(maxValue >= 1. - 1.e-5);
    // mean
    double mean = sum * norm;
    REQUIRE(almostEqual(mean, 0.5, 1.e-3));
    // variance
    double variance = sumSqr * norm - sqr(mean);
    REQUIRE(almostEqual(variance, 0.08333333333, 1.e-3));
    // third moment
    REQUIRE(almostEqual(sumCb * norm, 0.25, 1.e-3));
    // third central moment
    double thirdCentral = sumCb * norm - 3. * mean * sumSqr * norm + 2. * pow<3>((Float)mean);
    REQUIRE(almostEqual(thirdCentral, 0., 1.e-3));
}

TEST_CASE("UniformRng", "[rng]") {
    testRng(UniformRng());
}

TEST_CASE("HaltonQrng", "[rng]") {
    testRng(HaltonQrng());
}

TEST_CASE("BenzAsphaugRng", "[rng]") {
    testRng(BenzAsphaugRng(1234));
    // first few numbers with seed 1234
    BenzAsphaugRng rng(1234);
    const Float eps = 1.e-6_f;
    REQUIRE(almostEqual(rng(), 0.655416369, eps));
    REQUIRE(almostEqual(rng(), 0.200995207, eps));
    REQUIRE(almostEqual(rng(), 0.893622458, eps));
    REQUIRE(almostEqual(rng(), 0.281886548, eps));
    REQUIRE(almostEqual(rng(), 0.525000393, eps));
    REQUIRE(almostEqual(rng(), 0.314126790, eps));
}
