#include "math/rng/Rng.h"
#include "catch.hpp"
#include "math/MathUtils.h"
#include "post/Analysis.h"
#include "tests/Approx.h"

using namespace Sph;

template <typename TRng>
void testRng(TRng rng) {
    double minValue = 1000., maxValue = -1000.;
    double sum = 0.;
    double sumSqr = 0.;
    double sumCb = 0.;
    int N = 1000000;
    double norm = 1. / double(N);
    for (int i = 0; i < N; ++i) {
        double value = rng(0);
        minValue = min(minValue, value);
        maxValue = max(maxValue, value);
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
    REQUIRE(mean == approx(0.5, 1.e-3));
    // variance
    double variance = sumSqr * norm - sqr(mean);
    REQUIRE(variance == approx(0.08333333333, 1.e-3));
    // third moment
    REQUIRE(sumCb * norm == approx(0.25, 1.e-3));
    // third central moment
    double thirdCentral = sumCb * norm - 3. * mean * sumSqr * norm + 2. * pow<3>((Float)mean);
    REQUIRE(thirdCentral == approx(0., 1.e-3));
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
    REQUIRE(rng() == approx(0.655416369, eps));
    REQUIRE(rng() == approx(0.200995207, eps));
    REQUIRE(rng() == approx(0.893622458, eps));
    REQUIRE(rng() == approx(0.281886548, eps));
    REQUIRE(rng() == approx(0.525000393, eps));
    REQUIRE(rng() == approx(0.314126790, eps));
}

template <typename TExpected>
static void testDistribution(Array<Float>& values,
    const Float eps,
    const bool discrete,
    const TExpected& distribution) {
    Array<Post::HistPoint> points;
    Float sum = 0._f;
    if (discrete) {
        points.resize(Size(*std::max_element(values.begin(), values.end())) + 1);
        for (Size i = 0; i < points.size(); ++i) {
            points[i].count = 0;
            points[i].value = i;
        }
        for (Float value : values) {
            const Size i = Size(value);
            points[i].count++;
        }
        sum = values.size();

    } else {
        Post::HistogramParams params;
        // for smooth functions, we can use small number of bins to boost accuracy
        params.binCnt = 100;

        points = Post::getDifferentialHistogram(values, params);
        const Float dx = points[1].value - points[0].value;
        for (Post::HistPoint p : points) {
            sum += p.count * dx;
        }
    }

    Float actual = 0._f, expected = 0._f;
    for (Post::HistPoint p : points) {
        actual = Float(p.count) / sum;
        expected = distribution(p.value);

        if (actual != approx(expected, eps)) {
            break;
        }
    }
    // will print the offending values if test fails
    REQUIRE(actual == approx(expected, eps));
}

TEST_CASE("Sample normal distribution", "[rng]") {
    UniformRng rng;
    Array<Float> values;
    Float mu = 5._f;
    Float sigma = 1.5_f;

    for (Size i = 0; i < 10000000; ++i) {
        values.push(sampleNormalDistribution(rng, mu, sigma));
    }

    testDistribution(values, 1.e-3_f, false, [mu, sigma](const Float x) {
        return 1._f / sqrt(2._f * PI * sqr(sigma)) * Sph::exp(-sqr(x - mu) / (2._f * sqr(sigma)));
    });
}

TEST_CASE("Sample exponential distribution", "[rng]") {
    UniformRng rng;
    Array<Float> values;
    Float lambda = 2.8_f;

    for (Size i = 0; i < 10000000; ++i) {
        values.push(sampleExponentialDistribution(rng, lambda));
    }

    testDistribution(values, 2.e-3_f, false, [lambda](const Float x) { return lambda * Sph::exp(-lambda * x); });
}

TEST_CASE("Sample Poisson distribution", "[rng]") {
    UniformRng rng;
    Array<Float> values;
    Float lambda = 15._f;

    for (Size i = 0; i < 1000000; ++i) {
        values.push(samplePoissonDistribution(rng, lambda));
    }

    testDistribution(values, 0.01_f, true, [lambda](const Float x) {
        return pow(lambda, x) * Sph::exp(-lambda) / std::tgamma(x + 1);
    });
}
