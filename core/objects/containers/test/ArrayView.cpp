#include "catch.hpp"
#include "math/MathUtils.h"
#include "objects/containers/Array.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("ArrayView default construct", "[arrayview]") {
    ArrayView<int> view;
    REQUIRE(view.empty());
    REQUIRE(view.size() == 0);
    REQUIRE_SPH_ASSERT(view[0]);

    REQUIRE_NOTHROW([view] {
        for (int i : view) {
            throw i;
        }
    }());
}

TEST_CASE("ArrayView subset", "[arrayview]") {
    Array<int> data({ 1, 2, 3, 4, 5, 6 });
    ArrayView<int> a(data);
    REQUIRE(a.subset(0, 0).empty());
    REQUIRE(a.subset(0, 1) == Array<int>({ 1 }).view());
    REQUIRE(a.subset(1, 3) == Array<int>({ 2, 3, 4 }).view());
    REQUIRE(a.subset(4, 2) == Array<int>({ 5, 6 }).view());
    REQUIRE_SPH_ASSERT(a.subset(4, 3));
    REQUIRE_SPH_ASSERT(a.subset(-1, 2));
}

TEST_CASE("getSingleValueView", "[arrayview]") {
    int value = 5;
    ArrayView<int> a = getSingleValueView(value);
    REQUIRE(a.size() == 1);
    REQUIRE(a[0] == 5);
    a[0] = 3;
    REQUIRE(value == 3);
}

TEST_CASE("ArrayView almostEqual", "[arrayview]") {
    Array<float> a1({ 2.f, 4.f, 3.f });
    Array<float> a2({ 2.1f, 4.f, 3.f });
    Array<float> a3({ 2.f, 4.f });
    REQUIRE(almostEqual(a1.view(), a1.view(), 0));
    REQUIRE(almostEqual(a1.view(), a2.view(), 0.1f));
    REQUIRE(almostEqual(a2.view(), a1.view(), 0.1f));
    REQUIRE_FALSE(almostEqual(a1.view(), a2.view(), 0.02f));
    REQUIRE_FALSE(almostEqual(a2.view(), a1.view(), 0.02f));
    REQUIRE_FALSE(almostEqual(a1.view(), a3.view(), EPS));
    REQUIRE_FALSE(almostEqual(a3.view(), a1.view(), EPS));
}
