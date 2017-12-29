#include "post/StatisticTests.h"
#include "catch.hpp"
#include "math/rng/Rng.h"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Complete Correlation", "[statistictests]") {
    UniformRng rng;
    Array<PlotPoint> values;
    for (Size i = 0; i < 1000; ++i) {
        const Float x = rng();
        values.push(PlotPoint(x, x));
    }
    REQUIRE(Post::correlationCoefficient(values) == 1._f);
}

TEST_CASE("Uncorrelated values", "[statistictests]") {
    UniformRng rng;
    Array<PlotPoint> values;
    for (Size i = 0; i < 1000; ++i) {
        values.push(PlotPoint(rng(), rng()));
    }
    REQUIRE(Post::correlationCoefficient(values) == approx(0._f, 0.01_f));
}

TEST_CASE("Kolmogorov-Smirnov Distribution", "[statistictests]") {
    REQUIRE(Post::kolmogorovSmirnovDistribution(0) == 1._f);
    REQUIRE(Post::kolmogorovSmirnovDistribution(LARGE) == 0._f);

    Float Q = Post::kolmogorovSmirnovDistribution(2._f);
    REQUIRE(Q >= 0);
    REQUIRE(Q <= 1);

    Q = Post::kolmogorovSmirnovDistribution(1.e-6_f);
    REQUIRE(Q == approx(1._f, 1.e-6_f));
}

TEST_CASE("Kolmogorov-Smirnov 2D success", "[statistictests]") {
    UniformRng rng;
    Array<PlotPoint> values;
    for (Size i = 0; i < 1000; ++i) {
        values.push(PlotPoint(rng(), rng()));
    }

    Post::KsFunction expected = Post::getUniformKsFunction(Interval(0._f, 1._f), Interval(0._f, 1._f));
    Post::KsResult result = Post::kolmogorovSmirnovTest(values, expected);
    REQUIRE(result.D < 0.03_f);
    REQUIRE(result.prob > 0.8_f);
}

TEST_CASE("Kolmogorov-Smirnov 2D fail", "[statistictests]") {
    UniformRng rng;
    Array<PlotPoint> values;
    for (Size i = 0; i < 1000; ++i) {
        values.push(PlotPoint(rng(), sqrt(rng())));
    }

    Post::KsFunction expected = Post::getUniformKsFunction(Interval(0._f, 1._f), Interval(0._f, 1._f));
    Post::KsResult result = Post::kolmogorovSmirnovTest(values, expected);
    REQUIRE(result.D > 0.2_f);
    REQUIRE(result.prob < 0.001_f);
}
