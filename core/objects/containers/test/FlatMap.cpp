#include "objects/containers/FlatMap.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

/// Sanity check that the elements are really stored sorted
template <typename TKey, typename TValue>
static bool isSorted(const FlatMap<TKey, TValue>& map) {
    if (map.empty()) {
        return true;
    }
    TKey previous = map.begin()->key;
    for (auto iter = map.begin() + 1; iter < map.end(); ++iter) {
        auto& element = *iter;
        if (element.key <= previous) {
            return false;
        }
        previous = element.key;
    }
    return true;
}

static FlatMap<int, RecordType> getRandomMap() {
    FlatMap<int, RecordType> map;
    using Elem = typename decltype(map)::Element;

    Array<Elem> elements;
    for (int i = 0; i < 1000; ++i) {
        // create some non-trivial elements; key must be unique, value doesn't
        elements.emplaceBack(Elem{ i - 500, RecordType((i + 200) % 350) });
    }
    // random shuffle
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(elements.begin(), elements.end(), g);

    // add all of these elements to the map
    for (auto& e : elements) {
        map.insert(e.key, e.value);
    }
    return map;
}

TEST_CASE("Map default construct", "[flatmap]") {
    RecordType::resetStats();
    FlatMap<int, RecordType> map;
    REQUIRE(RecordType::constructedNum == 0);
    REQUIRE(map.size() == 0);
    REQUIRE(map.empty());
    REQUIRE(map.begin() == map.end());
    REQUIRE_FALSE(map.tryGet(5));
    REQUIRE_FALSE(map.contains(2));
    REQUIRE_SPH_ASSERT(map[0]);
}

TEST_CASE("Map insert lower key", "[flatmap]") {
    RecordType::resetStats();
    FlatMap<int, RecordType> map;
    map.insert(5, RecordType(2));
    REQUIRE(RecordType::existingNum() == 1); // some temporaries were constructed, but none survived
    REQUIRE(map.size() == 1);
    REQUIRE_FALSE(map.empty());
    REQUIRE_FALSE(map.contains(0));
    REQUIRE(map.contains(5));
    REQUIRE(map[5].value == 2);

    map.insert(2, RecordType(4));
    REQUIRE(RecordType::existingNum() == 2);
    REQUIRE(map.size() == 2);
    REQUIRE_FALSE(map.empty());
    REQUIRE(map.contains(2));
    REQUIRE(map.contains(5));
    REQUIRE_SPH_ASSERT(map[0]);
    REQUIRE(map[2].value == 4);
    REQUIRE_SPH_ASSERT(map[4]);
    REQUIRE(map[5].value == 2);

    REQUIRE(isSorted(map));
}

TEST_CASE("Map insert greater key", "[flatmap]") {
    FlatMap<int, RecordType> map;
    map.insert(5, RecordType(2));
    map.insert(8, RecordType(4));
    REQUIRE(RecordType::existingNum() == 2);
    REQUIRE(map.size() == 2);
    REQUIRE_FALSE(map.empty());
    REQUIRE(map.contains(5));
    REQUIRE(map.contains(8));
    REQUIRE_SPH_ASSERT(map[0]);
    REQUIRE(map[5].value == 2);
    REQUIRE_SPH_ASSERT(map[4]);
    REQUIRE(map[8].value == 4);

    REQUIRE(isSorted(map));
}

TEST_CASE("Map insert duplicate", "[flatmap]") {
    RecordType::resetStats();
    FlatMap<int, RecordType> map;
    map.insert(2, RecordType(3));
    map.insert(3, RecordType(4));
    map.insert(2, RecordType(1));
    REQUIRE(RecordType::existingNum() == 2);
    REQUIRE(map.size() == 2);
    REQUIRE(map.contains(2));
    REQUIRE(map.contains(3));
    REQUIRE(map[2].value == 1);
    REQUIRE(map[3].value == 4);

    map.insert(3, RecordType(5));
    REQUIRE(map[2].value == 1);
    REQUIRE(map[3].value == 5);
}

TEST_CASE("Map insert multiple", "[flatmap]") {
    RecordType::resetStats();
    FlatMap<int, RecordType> map = getRandomMap();

    REQUIRE(RecordType::existingNum());
    REQUIRE(map.size() == 1000);
    REQUIRE(isSorted(map));

    REQUIRE([&] {
        for (int i = 0; i < 1000; ++i) {
            if (map[i - 500].value != (i + 200) % 350) {
                return false;
            }
        }
        return true;
    }());
    REQUIRE_SPH_ASSERT(map[-501]);
    REQUIRE_SPH_ASSERT(map[500]);
}

TEST_CASE("Map insert equivalent", "[flatmap]") {
    struct TestLess {
        bool operator()(const int i1, const int i2) const {
            // considers 2 and 3 equivalent
            if ((i1 == 2 && i2 == 3) || (i1 == 3 && i2 == 2)) {
                return false;
            } else {
                return i1 < i2;
            }
        }
    };

    FlatMap<int, RecordType, TestLess> map;
    map.insert(1, RecordType(5));
    map.insert(2, RecordType(6));
    map.insert(3, RecordType(7));
    map.insert(4, RecordType(8));

    REQUIRE(map.size() == 3);
    REQUIRE(map[1].value == 5);
    REQUIRE(map[2].value == 7);
    REQUIRE(map[3].value == 7);
    REQUIRE(map[4].value == 8);
    REQUIRE(&map[2] == &map[3]);
}

TEST_CASE("Map remove", "[flatmap]") {
    RecordType::resetStats();
    FlatMap<int, RecordType> map;
    map.insert(5, RecordType(1));
    map.remove(5);
    REQUIRE(RecordType::existingNum() == 0);
    REQUIRE(map.size() == 0);
    REQUIRE(map.empty());

    map.insert(2, RecordType(4));
    map.insert(5, RecordType(3));
    REQUIRE_SPH_ASSERT(map.remove(3));
    map.remove(5);
    REQUIRE(map.size() == 1);
    REQUIRE_SPH_ASSERT(map[5]);
    REQUIRE(map[2].value == 4);

    map.insert(1, RecordType(6));
    map.remove(1);
    REQUIRE(map.size() == 1);
    REQUIRE_SPH_ASSERT(map[1]);
    REQUIRE(map[2].value == 4);
}

TEST_CASE("Map remove multiple", "[flatmap]") {
    RecordType::resetStats();
    FlatMap<int, RecordType> map = getRandomMap();

    Array<int> indices(1000);
    for (int i = 0; i < int(indices.size()); ++i) {
        indices[i] = i - 500;
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

    REQUIRE([&] {
        for (int i = 0; i < int(indices.size()); ++i) {
            map.remove(indices[i]);
            if (map.size() != indices.size() - i - 1) {
                return false;
            }
            if (!isSorted(map)) {
                return false;
            }
        }
        return true;
    }());
}

TEST_CASE("Map tryRemove", "[flatmap]") {
    FlatMap<int, RecordType> map;
    map.insert(3, RecordType(5));
    map.insert(5, RecordType(1));
    REQUIRE(map.tryRemove(3));
    REQUIRE(map.size() == 1);
    REQUIRE(map[5].value == 1);
    REQUIRE_FALSE(map.tryRemove(6));
    REQUIRE_FALSE(map.tryRemove(4));
    REQUIRE(map.size() == 1);
    REQUIRE(map[5].value == 1);
    REQUIRE(map.tryRemove(5));
    REQUIRE(map.empty());
}

TEST_CASE("Map tryGet", "[flatmap]") {
    FlatMap<int, RecordType> map;
    map.insert(4, RecordType(9));
    map.insert(5, RecordType(2));
    map.insert(1, RecordType(4));
    REQUIRE(map.tryGet(4)->value == 9);
    REQUIRE(map.tryGet(5)->value == 2);
    REQUIRE(map.tryGet(1)->value == 4);
    REQUIRE_FALSE(map.tryGet(2));
    REQUIRE_FALSE(map.tryGet(3));
}

TEST_CASE("Map iterators", "[flatmap]") {
    FlatMap<int, RecordType> map = getRandomMap();
    Size counter = 0;
    REQUIRE([&] {
        for (FlatMap<int, RecordType>::Element element : map) {
            ++counter;
            if (map[element.key].value != element.value.value) {
                return false;
            }
        }
        return counter == 1000;
    }());
}

TEST_CASE("Map arrayview", "[flatmap]") {
    FlatMap<int, RecordType> map;
    map.insert(5, RecordType(1));
    map.insert(-1, RecordType(3));
    map.insert(0, RecordType(5));

    ArrayView<FlatMap<int, RecordType>::Element> view = map;
    REQUIRE(view.size() == 3);
    REQUIRE(view[0].key == -1);
    REQUIRE(view[0].value.value == 3);
    REQUIRE(view[1].key == 0);
    REQUIRE(view[1].value.value == 5);
    REQUIRE(view[2].key == 5);
    REQUIRE(view[2].value.value == 1);
}
