#include "math/Means.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

template <typename TMean, typename... TArgs>
void testMean(const Float expectedMean, const Float eps, TArgs&&... args) {
    TMean mean(std::forward<TArgs>(args)...);
    for (Float f = 1._f; f <= 10._f; f += 1._f) {
        mean.accumulate(f);
    }
    REQUIRE(mean.compute() == approx(expectedMean, eps));
}

TEST_CASE("GeneralizedMeans", "[math]") {
    testMean<ArithmeticMean>(5.5_f, EPS);
    testMean<HarmonicMean>(25200._f / 7381._f, EPS);
    testMean<GeometricMean>(4.528728688116764762203309f, 1.e-6_f);
}

TEST_CASE("RuntimeMeans", "[math]") {
    testMean<PositiveMean>(5.5_f, EPS, 1._f);
    testMean<NegativeMean>(25200._f / 7381._f, 1.e-6_f, -1._f);
}
