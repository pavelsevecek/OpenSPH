#include "math/rng/VectorRng.h"
#include "catch.hpp"
#include "math/Math.h"
#include "geometry/Bounds.h"
#include <iostream>
using namespace Sph;

template <typename TVectorRng>
void testVectorRng(TVectorRng&& rng,
                   Vector expectedMin,
                   Vector expectedMax,
                   Vector expectedMean,
                   Vector expectedVariance) {
    Vector sum(0.);
    Vector sumSqr(0.);
    Vector minValues(1000.);
    Vector maxValues(-1000.);

    int N       = 1000000;
    double norm = 1. / double(N);
    for (int i = 0; i < N; ++i) {
        Vector values = rng();
        minValues     = Math::min(minValues, values);
        maxValues     = Math::max(maxValues, values);
        sum += values;
        sumSqr += Math::sqr(values);
    }
    Vector mean = sum * norm;
    for (int i = 0; i < 3; ++i) {
        REQUIRE(minValues[i] >= expectedMin[i]);
        REQUIRE(maxValues[i] <= expectedMax[i]);
        REQUIRE(Math::almostEqual(mean[i], expectedMean[i], 1.e-2));
        REQUIRE(Math::almostEqual(sumSqr[i] * norm - Math::sqr(mean[i]), expectedVariance[i], 1.e-2));
    }
}

TEST_CASE("VectorRng", "[vectorrng]") {
    testVectorRng(VectorRng<UniformRng>(), Vector(0.), Vector(1.), Vector(0.5), Vector(0.0833));
}

TEST_CASE("VectorPdfRng", "[vectorrng]") {
    /// uniform RNG inside the box
    Box box1(Vector(2.5), Vector(3.7));
    auto rng1 = makeVectorPdfRng(box1, UniformRng());
    testVectorRng(rng1, Vector(2.5), Vector(3.7), Vector(3.1), Vector(1.44 * 0.0833));

    /// Linear PDF in one dimension, constant in rest
    Box box2(Vector(0.), Vector(1.));
    auto rng2 = makeVectorPdfRng(box2, UniformRng(), [](const Vector& v) { return v[X]; });
    testVectorRng(rng2,
                  Vector(0., 0., 0.),
                  Vector(1., 1., 1.),
                  Vector(0.6666, 0.5, 0.5),
                  Vector(0.0555, 0.0833, 0.0833));
}
