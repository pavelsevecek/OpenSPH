#include "math/rng/VectorRng.h"
#include "catch.hpp"
#include "geometry/Box.h"
#include "math/Math.h"
#include "utils/Approx.h"

using namespace Sph;

template <typename TVectorRng>
void testVectorRng(TVectorRng&& rng,
    Vector expectedMin,
    Vector expectedMax,
    Vector expectedMean,
    Vector expectedVariance) {
    Vector sum(0._f);
    Vector sumSqr(0._f);
    Vector minValues(INFTY);
    Vector maxValues(-INFTY);

    int N = 100000;
    double norm = 1. / double(N);
    for (int i = 0; i < N; ++i) {
        Vector values = rng();
        minValues = min(minValues, values);
        maxValues = max(maxValues, values);
        sum += values;
        sumSqr += sqr(values);
    }
    Vector mean = sum * norm;
    for (int i = 0; i < 3; ++i) {
        REQUIRE(minValues[i] >= expectedMin[i]);
        REQUIRE(maxValues[i] <= expectedMax[i]);
        REQUIRE(mean[i] == approx(expectedMean[i], 1.e-2));
        REQUIRE(sumSqr[i] * norm - sqr(mean[i]) == approx(expectedVariance[i], 1.e-2));
    }
}

TEST_CASE("VectorRng", "[vectorrng]") {
    testVectorRng(VectorRng<UniformRng>(), Vector(0.), Vector(1.), Vector(0.5), Vector(0.0833));
}

TEST_CASE("VectorPdfRng", "[vectorrng]") {
    /// uniform RNG inside the box
    Box box1(Vector(2.5), Vector(3.7));
    VectorPdfRng<UniformRng> rng1(box1);
    testVectorRng(rng1, Vector(2.5), Vector(3.7), Vector(3.1), Vector(1.44 * 0.0833));

    /// Linear PDF in one dimension, constant in rest
    Box box2(Vector(0.), Vector(1.));
    VectorPdfRng<UniformRng> rng2(box2, [](const Vector& v) { return v[X]; });
    testVectorRng(rng2,
        Vector(0., 0., 0.),
        Vector(1., 1., 1.),
        Vector(0.6666, 0.5, 0.5),
        Vector(0.0555, 0.0833, 0.0833));
}
