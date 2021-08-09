#include "objects/utility/Algorithm.h"
#include "catch.hpp"
#include "objects/containers/Array.h"

using namespace Sph;

TEST_CASE("FindIf", "[algorithm]") {
    Array<int> data = { 3, 4, 2 };
    Iterator<int> iter = findIf(data, [](int v) { return v % 2 == 0; });
    REQUIRE(iter != data.end());
    REQUIRE(*iter == 4);

    data = Array<int>();
    REQUIRE(findIf(data, [](int) { return true; }) == data.end());
}

TEST_CASE("Contains", "[algorithm") {
    Array<int> data = { 4, 3, 2 };
    REQUIRE(contains(data, 2));
    REQUIRE(contains(data, 3));
    REQUIRE(contains(data, 4));
    REQUIRE_FALSE(contains(data, 1));
    REQUIRE_FALSE(contains(data, 5));

    data = Array<int>();
    REQUIRE_FALSE(contains(data, 2));
}

TEST_CASE("CountIf", "[algorithm]") {
    Array<float> data = { -4.f, -3.f, 0.f, 1.f, 2.f, 10.f, 7.f };
    int even = countIf(data, [](const float v) { return int(v) % 2 == 0; });
    REQUIRE(even == 4);
    int negative = countIf(data, [](const float v) { return v < 0; });
    REQUIRE(negative == 2);

    data.clear();
    REQUIRE(countIf(data, [](const float v) { return v == 0.f; }) == 0);
}

TEST_CASE("AllUnique", "[algorithm]") {
    Array<int> data1{ 1, 2, 6, 3, 5 };
    REQUIRE(allUnique(data1));

    Array<int> data2{ 3, 2, 6, 3, 5 };
    REQUIRE_FALSE(allUnique(data2));

    REQUIRE(allUnique({ 4, 5, 1, 2 }));
    REQUIRE_FALSE(allUnique({ 4, 5, 1, 4 }));

    REQUIRE(allUnique(Array<int>{}));
}

TEST_CASE("AnyCommon", "[algorithm]") {
    Array<int> data1 = { 2, 4, 6, 8 };
    Array<int> data2 = { 3, 6, 9 };
    Array<int> data3 = { 5, 15, 25 };
    REQUIRE(anyCommon(data1, data2));
    REQUIRE_FALSE(anyCommon(data1, data3));
    REQUIRE_FALSE(anyCommon(data2, data3));
}

TEST_CASE("Ranges almostEqual", "[algorithm]") {
    Array<float> a1({ 2.f, 4.f, 3.f });
    Array<float> a2({ 2.1f, 4.f, 3.f });
    Array<float> a3({ 2.f, 4.f });
    REQUIRE(almostEqual(a1, a1, 0));
    REQUIRE(almostEqual(a1, a2, 0.1f));
    REQUIRE(almostEqual(a2, a1, 0.1f));
    REQUIRE_FALSE(almostEqual(a1, a2, 0.02f));
    REQUIRE_FALSE(almostEqual(a2, a1, 0.02f));
    REQUIRE_FALSE(almostEqual(a1, a3, EPS));
    REQUIRE_FALSE(almostEqual(a3, a1, EPS));
}
