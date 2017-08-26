#include "objects/containers/Map.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

/// Sanity check that the elements are really stored sorted
template <typename TKey, typename TValue>
static bool isSorted(const Map<TKey, TValue>& map) {
    TKey previous;
    for (auto& element : map) {
        if (element.key == map.begin()->key) { // first
            continue;
        }
        if (element.key <= previous) {
            return false;
        }
        previous = element.key;
    }
    return true;
}

static Map<int, RecordType> getRandomMap() {
    Map<int, RecordType> map;
    using Elem = decltype(map)::Element;

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

TEST_CASE("Map default construct", "[map]") {
    RecordType::resetStats();
    Map<int, RecordType> map;
    REQUIRE(RecordType::constructedNum == 0);
    REQUIRE(map.size() == 0);
    REQUIRE(map.empty());
    REQUIRE(map.begin() == map.end());
    REQUIRE_FALSE(map.tryGet(5));
    REQUIRE_FALSE(map.contains(2));
    REQUIRE_ASSERT(map[0]);
}

TEST_CASE("Map insert", "[map]") {
    RecordType::resetStats();
    Map<int, RecordType> map;
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
    REQUIRE_ASSERT(map[0]);
    REQUIRE(map[2].value == 4);
    REQUIRE_ASSERT(map[4]);
    REQUIRE(map[5].value == 2);

    REQUIRE(isSorted(map));
}

TEST_CASE("Map insert multiple", "[map]") {
    Map<int, RecordType> map = getRandomMap();

    REQUIRE(map.size() == 1000);
    REQUIRE(isSorted(map));
}

/// \todo add benchmark against std::map?

TEST_CASE("Map remove", "[map]") {}

TEST_CASE("Map remove multiple", "[map]") {}

TEST_CASE("Map tryGet", "[map]") {}

TEST_CASE("Map iterators", "[map]") {}

TEST_CASE("Map arrayview", "[map]") {}
