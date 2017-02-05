#include "objects/wrappers/Iterators.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
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

TEST_CASE("ReverseWrapper", "[iterators]") {
    Array<Size> data;
    auto empty = reverse(data);
    REQUIRE(empty.begin() == empty.end());
    REQUIRE(empty.size() == 0);

    data = Array<Size>{ 1, 2, 3, 4, 5 };
    auto wrapper = reverse(data);
    REQUIRE(wrapper.size() == 5);
    auto iter = wrapper.begin();
    REQUIRE(*iter == 5);
    ++iter;
    REQUIRE(*iter == 4);
    ++iter;
    REQUIRE(*iter == 3);
    ++iter;
    REQUIRE(*iter == 2);
    ++iter;
    REQUIRE(*iter == 1);
    ++iter;
    REQUIRE(iter == wrapper.end());
}
