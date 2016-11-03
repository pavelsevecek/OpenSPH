#include "sph/initconds/InitConds.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"

using namespace Sph;

void testDistribution(Abstract::Distribution* distribution) {
    BlockDomain domain(Vector(-3._f), Vector(2._f));
    Array<Vector> values = distribution->generate(1000, &domain);
    REQUIRE(values.size() > 900);
    REQUIRE(values.size() < 1100);
    bool allInside = areAllMatching<Vector>(values, [&](const Vector& v) { return domain.isInside(v); });
    REQUIRE(allInside);
}

TEST_CASE("HexaPacking", "[initconds]") {
    HexagonalPacking packing;
    testDistribution(&packing);
}

TEST_CASE("CubicPacking", "[initconds]") {
    CubicPacking packing;
    testDistribution(&packing);
}

TEST_CASE("RandomDistribution", "[initconds]") {
    RandomDistribution random;
    testDistribution(&random);


    // 100 points inside block [0,1]^d, approx. distance is 100^(-1/d)
}
