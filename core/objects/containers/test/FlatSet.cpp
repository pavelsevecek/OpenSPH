#include "objects/containers/FlatSet.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Set default construct", "[flatset]") {
    RecordType::resetStats();
    FlatSet<RecordType> set;
    REQUIRE(RecordType::constructedNum == 0);
    REQUIRE(set.size() == 0);
    REQUIRE(set.empty());
    REQUIRE(set.begin() == set.end());
    // REQUIRE_SPH_ASSERT(set[0]); /// \todo causes terminate, wtf?
}

TEST_CASE("Set initializer_list common", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_COMMON, { 1, 5, 3, 4, 3 });
    REQUIRE(set.size() == 4);
    REQUIRE_FALSE(set.empty());
    REQUIRE(set[0].value == 1);
    REQUIRE(set[1].value == 3);
    REQUIRE(set[2].value == 4);
    REQUIRE(set[3].value == 5);

    set = FlatSet<RecordType>(ELEMENTS_COMMON, { 4, 4, 4, 4, 4, 4, 4, 4, 4 });
    REQUIRE(set.size() == 1);
    REQUIRE(set[0].value == 4);

    set = FlatSet<RecordType>(ELEMENTS_COMMON, {});
    REQUIRE(set.size() == 0);
    REQUIRE(set.empty());
}

TEST_CASE("Set initializer_list unique", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_UNIQUE, { 2, 5, 4 });
    REQUIRE(set.size() == 3);
    REQUIRE(set[0].value == 2);
    REQUIRE(set[1].value == 4);
    REQUIRE(set[2].value == 5);

    set = FlatSet<RecordType>(ELEMENTS_UNIQUE, { 1 });
    REQUIRE(set.size() == 1);
    REQUIRE(set[0].value == 1);

    set = FlatSet<RecordType>(ELEMENTS_UNIQUE, {});
    REQUIRE(set.size() == 0);

    REQUIRE_SPH_ASSERT(FlatSet<RecordType>(ELEMENTS_UNIQUE, { 4, 2, 4 }));
}

TEST_CASE("Set initializer_list sorted and unique", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_SORTED_UNIQUE, { 6, 7, 8, 10 });
    REQUIRE(set.size() == 4);
    REQUIRE(set[0].value == 6);
    REQUIRE(set[1].value == 7);
    REQUIRE(set[2].value == 8);
    REQUIRE(set[3].value == 10);

    REQUIRE_SPH_ASSERT(FlatSet<RecordType>(ELEMENTS_SORTED_UNIQUE, { 4, 5, 5, 6 }));
    REQUIRE_SPH_ASSERT(FlatSet<RecordType>(ELEMENTS_SORTED_UNIQUE, { 4, 5, 6, 4 }));
}

TEST_CASE("Set array common", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_COMMON, Array<RecordType>({ 4, 3, 3, 2, 2, 5 }));
    REQUIRE(set.size() == 4);
    REQUIRE(set[0].value == 2);
    REQUIRE(set[1].value == 3);
    REQUIRE(set[2].value == 4);
    REQUIRE(set[3].value == 5);

    set = FlatSet<RecordType>(ELEMENTS_COMMON, Array<RecordType>());
    REQUIRE(set.empty());
}

TEST_CASE("Set array unique", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_UNIQUE, Array<RecordType>({ 4, 3, 2 }));
    REQUIRE(set.size() == 3);
    REQUIRE(set[0].value == 2);
    REQUIRE(set[1].value == 3);
    REQUIRE(set[2].value == 4);

    REQUIRE_SPH_ASSERT(FlatSet<RecordType>(ELEMENTS_UNIQUE, Array<RecordType>({ 4, 3, 3 })));
}

TEST_CASE("Set insert", "[flatset]") {
    FlatSet<RecordType> set;
    REQUIRE(set.insert(5));
    REQUIRE(set.size() == 1);
    REQUIRE(set[0].value == 5);

    REQUIRE(set.insert(3));
    REQUIRE(set.size() == 2);
    REQUIRE(set[0].value == 3);
    REQUIRE(set[1].value == 5);

    REQUIRE(set.insert(7));
    REQUIRE(set.size() == 3);
    REQUIRE(set[0].value == 3);
    REQUIRE(set[1].value == 5);
    REQUIRE(set[2].value == 7);

    REQUIRE_FALSE(set.insert(5));
    REQUIRE_FALSE(set.insert(3));
    REQUIRE(set.size() == 3);
    REQUIRE(set[0].value == 3);
    REQUIRE(set[1].value == 5);
    REQUIRE(set[2].value == 7);
}

TEST_CASE("Set insert range", "[flatset]") {
    FlatSet<int> set(ELEMENTS_UNIQUE, { 1, 5, 9 });
    Array<int> values = { 2, 10 };
    set.insert(values.begin(), values.end());
    REQUIRE(ArrayView<int>(set) == Array<int>({ 1, 2, 5, 9, 10 }).view());

    values = { 3, 5, 3, 3, 1 };
    set.insert(values.begin(), values.end());
    REQUIRE(ArrayView<int>(set) == Array<int>({ 1, 2, 3, 5, 9, 10 }).view());

    Array<int> empty;
    set.insert(empty.begin(), empty.end());
    REQUIRE(ArrayView<int>(set) == Array<int>({ 1, 2, 3, 5, 9, 10 }).view());

    FlatSet<int> emptySet;
    emptySet.insert(empty.begin(), empty.end());
    REQUIRE(emptySet.empty());

    values = { 1, 2 };
    emptySet.insert(values.begin(), values.end());
    REQUIRE(ArrayView<int>(emptySet) == Array<int>({ 1, 2 }).view());
}

TEST_CASE("Set find", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_UNIQUE, { 7, 4, 3, 5, 9 }); // 3, 4, 5, 7, 9
    auto iter = set.find(5);
    REQUIRE(iter != set.end());
    REQUIRE(iter - set.begin() == 2);
    REQUIRE(iter->value == 5);

    REQUIRE(set.find(1) == set.end());

    set = {};
    REQUIRE(set.empty());
    REQUIRE(set.find(7) == set.end());
}

TEST_CASE("Set erase", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_SORTED_UNIQUE, { 1, 2, 3, 4, 5 });
    set.erase(set.begin());
    REQUIRE(set.size() == 4);
    REQUIRE(set[0].value == 2);
    REQUIRE(set[1].value == 3);
    REQUIRE(set[2].value == 4);
    REQUIRE(set[3].value == 5);

    set.erase(set.begin() + 2);
    REQUIRE(set.size() == 3);
    REQUIRE(set[0].value == 2);
    REQUIRE(set[1].value == 3);
    REQUIRE(set[2].value == 5);

    REQUIRE(set.erase(set.begin() + 1) == set.begin() + 1);

    REQUIRE_SPH_ASSERT(set.erase(set.begin() + 3));
}

TEST_CASE("Set erase loop", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_SORTED_UNIQUE, { 1, 2, 3, 4, 5 });
    int index = 1;
    for (auto iter = set.begin(); iter != set.end();) {
        REQUIRE(iter->value == index);
        iter = set.erase(iter);
        REQUIRE(int(set.size()) == 5 - index);
        index++;
    }
    REQUIRE(index == 6);
    REQUIRE(set.empty());
}

TEST_CASE("Set view", "[flatset]") {
    FlatSet<RecordType> set(ELEMENTS_UNIQUE, { 5, 2, 7, 9 });
    REQUIRE(ArrayView<RecordType>(set) == ArrayView<RecordType>(Array<RecordType>{ 2, 5, 7, 9 }));
}
