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
    // REQUIRE_ASSERT(set[0]); /// \todo causes terminate, wtf?
}

TEST_CASE("Set initializer_list", "[flatset]") {
    FlatSet<RecordType> set{ 1, 5, 3, 4, 3 };
    REQUIRE(set.size() == 4);
    REQUIRE_FALSE(set.empty());
    REQUIRE(set[0].value == 1);
    REQUIRE(set[1].value == 3);
    REQUIRE(set[2].value == 4);
    REQUIRE(set[3].value == 5);
}

TEST_CASE("Set insert", "[flatset]") {
    FlatSet<RecordType> set;
    set.insert(5);
    REQUIRE(set.size() == 1);
    REQUIRE(set[0].value == 5);

    set.insert(3);
    REQUIRE(set.size() == 2);
    REQUIRE(set[0].value == 3);
    REQUIRE(set[1].value == 5);

    set.insert(7);
    REQUIRE(set.size() == 3);
    REQUIRE(set[0].value == 3);
    REQUIRE(set[1].value == 5);
    REQUIRE(set[2].value == 7);

    set.insert(5);
    set.insert(3);
    REQUIRE(set.size() == 3);
    REQUIRE(set[0].value == 3);
    REQUIRE(set[1].value == 5);
    REQUIRE(set[2].value == 7);
}

TEST_CASE("Set find", "[flatset]") {
    FlatSet<RecordType> set{ 7, 4, 3, 5, 9 }; // 3, 4, 5, 7, 9
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
    FlatSet<RecordType> set{ 1, 2, 3, 4, 5 };
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

    REQUIRE_ASSERT(set.erase(set.begin() + 3));
}

TEST_CASE("Set erase loop", "[flatset]") {
    FlatSet<RecordType> set{ 1, 2, 3, 4, 5 };
    Size index = 1;
    for (auto iter = set.begin(); iter != set.end();) {
        REQUIRE(iter->value == index);
        iter = set.erase(iter);
        REQUIRE(set.size() == 5 - index);
        index++;
    }
    REQUIRE(index == 6);
    REQUIRE(set.empty());
}

TEST_CASE("Set view", "[flatset]") {
    FlatSet<RecordType> set{ 5, 2, 7, 9 };
    REQUIRE(ArrayView<RecordType>(set) == ArrayView<RecordType>(Array<RecordType>{ 2, 5, 7, 9 }));
}