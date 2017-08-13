#include "math/Means.h"
#include "catch.hpp"
#include "tests/Approx.h"

using namespace Sph;

template <typename TMean>
void testMean(const Float expectedMean) {
    TMean mean;
    for (Float f = 1._f; f <= 10._f; f += 1._f) {
        mean.template accumulate(f);
    }
    REQUIRE(mean.template compute() == approx(expectedMean));
}

TEST_CASE("Means", "[math]") {
    testMean<ArithmeticMean>(5.5_f);
    testMean<HarmonicMean>(25200._f / 7381._f);
}
