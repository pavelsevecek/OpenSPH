#include "objects/wrappers/Iterators.h"
#include "objects/containers/Array.h"
#include "catch.hpp"
#include <algorithm>

using namespace Sph;

TEST_CASE("ComponentIterator", "[iterators]") {
    Array<Vector> data(3._f);
    data.fill(Vector(1._f));

    int i = 0;
    for (Float& iter : componentAdapter(data, 0)) {
        iter = Float(i++);
    }
    REQUIRE(data[0] == Vector(0.f, 1.f, 1.f));
    REQUIRE(data[1] == Vector(1.f, 1.f, 1.f));
    REQUIRE(data[2] == Vector(2.f, 1.f, 1.f));
    for (Float& iter : componentAdapter(data, 2)) {
        iter = 5.f - Float(i++);
    }
    REQUIRE(data[0] == Vector(0.f, 1.f, 2.f));
    REQUIRE(data[1] == Vector(1.f, 1.f, 1.f));
    REQUIRE(data[2] == Vector(2.f, 1.f, 0.f));

    auto adapter = componentAdapter(data, 2);
    std::sort(adapter.begin(), adapter.end());
    REQUIRE(data[0] == Vector(0.f, 1.f, 0.f));
    REQUIRE(data[1] == Vector(1.f, 1.f, 1.f));
    REQUIRE(data[2] == Vector(2.f, 1.f, 2.f));
}
