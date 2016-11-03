#include "math/rng/Rng.h"
#include "catch.hpp"
#include "math/Math.h"
#include <iostream>
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
        minValue     = Math::min(minValue, value);
        maxValue     = Math::max(maxValue, value);
        sum += value;
        sumSqr += Math::sqr(value);
        sumCb += Math::pow<3>(value);
    }
    REQUIRE(minValue >= 0.);
    REQUIRE(minValue <= 1.e-5);
    REQUIRE(maxValue <= 1.);
    REQUIRE(maxValue >= 1. - 1.e-5);
    // mean
    double mean = sum * norm;
    REQUIRE(Math::almostEqual(mean, 0.5, 1.e-3));
    // variance
    double variance = sumSqr * norm - Math::sqr(mean);
    REQUIRE(Math::almostEqual(variance, 0.08333333333, 1.e-3));
    // third moment
    REQUIRE(Math::almostEqual(sumCb * norm, 0.25, 1.e-3));
    // third central moment
    double thirdCentral = sumCb * norm - 3. * mean * sumSqr * norm + 2. * Math::pow<3>(mean);
    REQUIRE(Math::almostEqual(thirdCentral, 0., 1.e-3));
}

TEST_CASE("UniformRng", "[rng]") {
    testRng(UniformRng());
}

TEST_CASE("HaltonQrng", "[rng]") {
    testRng(HaltonQrng());
}
