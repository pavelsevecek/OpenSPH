#include "objects/utility/Iterators.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "objects/geometry/Vector.h"
#include "utils/Utils.h"
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

TEST_CASE("ReverseAdapter", "[iterators]") {
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

TEST_CASE("TupleAdapter", "[iterators]") {
    Array<float> floats{ 1.f, 2.f, 3.f, 4.f };
    Array<int> ints{ 1, 2, 3, 4 };
    Array<char> chars{ 'a', 'b', 'c', 'd' };

    struct Element {
        float& f;
        int& i;
        char ch;
    };
    Size cnt = 1;
    for (Element e : iterateTuple<Element>(floats, ints, chars)) {
        REQUIRE(e.f == float(cnt));
        REQUIRE(e.i == cnt);
        REQUIRE(e.ch == 'a' + char(cnt - 1));
        e.f = 6.f;
        e.i = 7;
        cnt++;
    };
    REQUIRE(cnt == 5);
    REQUIRE(floats == Array<float>({ 6.f, 6.f, 6.f, 6.f }));
    REQUIRE(ints == Array<int>({ 7, 7, 7, 7 }));

    REQUIRE_ASSERT([] {
        for (auto i : iterateTuple<Tuple<int, float>>(Array<int>{ 5 }, Array<float>{ 3.f, 4.f })) {
            (void)i;
        }
    }());

    REQUIRE_ASSERT([] {
        for (auto i : iterateTuple<Tuple<int, float, char>>(
                 Array<int>{ 5, 5 }, Array<float>{ 3.f, 4.f }, Array<char>{ 'a', 'e', 'f' })) {
            (void)i;
        }
    }());

    REQUIRE_ASSERT([] {
        for (auto i : iterateTuple<Tuple<int, float>>(Array<int>{ 5, 5 }, Array<float>{})) {
            (void)i;
        }
    }());
}

TEST_CASE("SubsetIterator", "[iterators]") {
    Array<int> values{ 2, 5, 4, 8, 3, -1, 2, 1 };
    Array<int> visited;
    for (float v : subset(values, [](int f) { return f % 2 == 0; })) {
        visited.push(v);
    }
    REQUIRE(visited == makeArray(2, 4, 8, 2));

    auto emptySubset = subset(values, [](int) { return false; });
    REQUIRE(emptySubset.begin() == emptySubset.end());

    auto oddNumbers = subset(values, [](int f) { return f % 2 == 1; });
    REQUIRE(*oddNumbers.begin() == 5);
}

TEST_CASE("IndexSequence", "[iterators]") {
    Size idx = 0;
    for (Size i : IndexSequence(0, 5)) {
        REQUIRE(i == idx);
        idx++;
    }
    REQUIRE(idx == 5);
}
