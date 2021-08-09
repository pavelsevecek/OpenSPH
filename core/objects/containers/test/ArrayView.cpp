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
