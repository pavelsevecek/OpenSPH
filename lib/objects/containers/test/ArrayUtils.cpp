#include "objects/containers/Array.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"

using namespace Sph;


TEST_CASE("Find", "[arrayutils]") {
    Array<float> storage = { -3.f, -2.f, -1.f, 0.f, 1.f, 2.f, 3.f };
    Iterator<float> i    = findByMinimum<float, float>(storage, [](const float v) { return v; });
    REQUIRE(*i == -3.f);
    i = findByMinimum<float, float>(storage, [](const float v) { return -v; });
    REQUIRE(*i == 3.f);
    i = findByMinimum<float, float>(storage, [](const float v) { return Math::abs(v); });
    REQUIRE(*i == 0.f);
}

TEST_CASE("FindPair", "[arrayutils]") {
    Array<float> storage = { -10.f, 5.f, -3.f, 0.f, 1.f, 12.f, 3.f };
    Iterator<float> i1, i2;
    tie(i1, i2) = findPairByMinimum<float, float>(storage, [](const float v1, const float v2) {
        return Math::abs(v1 - v2); // minimum distance
    });
    REQUIRE(*i1 == 0.f);
    REQUIRE(*i2 == 1.f);

    tie(i1, i2) = findPairByMaximum<float, float>(storage, [](const float v1, const float v2) {
        return Math::abs(v1 - v2); // maximu distance
    });
    REQUIRE(*i1 == -10.f);
    REQUIRE(*i2 == 12.f);
}

TEST_CASE("CountMatching", "[arrayutils]") {
    Array<float> storage = { -4.f, -3.f, 0.f, 1.f, 2.f, 10.f, 7.f };
    int even             = getCountMatching(storage, [](const float v) { return int(v) % 2 == 0; });
    REQUIRE(even == 4);
    int negative = getCountMatching(storage, [](const float v) { return v < 0; });
    REQUIRE(negative == 2);
}
