#include "objects/containers/Array.h"
#include "utils/Utils.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Construction", "[array]") {
    // default construction
    Array<float> ar1;
    REQUIRE(ar1.size() == 0);

    // initializer list construction
    Array<float> ar2{ 1.f, 2.f, 2.5f, 3.6f };
    REQUIRE(ar2.size() == 4);
    REQUIRE(ar2[0] == 1.f);
    REQUIRE(ar2[1] == 2.f);
    REQUIRE(ar2[2] == 2.5f);
    REQUIRE(ar2[3] == 3.6f);

    // move construction
    Array<float> ar3(std::move(ar2));
    REQUIRE(ar3.size() == 4);
    REQUIRE(ar3[2] == 2.5f);
    REQUIRE(ar2.size() == 0);
}

TEST_CASE("Resize", "[array]") {
    DummyStruct::constructedNum = 0;
    Array<DummyStruct> ar;
    REQUIRE(DummyStruct::constructedNum == 0);
    REQUIRE(ar.size() == 0);
    ar.resize(3);
    REQUIRE(DummyStruct::constructedNum == 3);
    REQUIRE(ar.size() == 3);
    ar.resize(5);
    REQUIRE(DummyStruct::constructedNum == 5);
    REQUIRE(ar.size() == 5);
    ar.resize(2);
    REQUIRE(DummyStruct::constructedNum == 2);
    REQUIRE(ar.size() == 2);
    ar.clear();
    REQUIRE(DummyStruct::constructedNum == 0);
    REQUIRE(ar.size() == 0);
}

TEST_CASE("Push & pop", "[array]") {
    DummyStruct::constructedNum = 0;
    Array<DummyStruct> ar;
    ar.push(DummyStruct(5));
    REQUIRE(DummyStruct::constructedNum == 1);
    REQUIRE(ar.size() == 1);
    ar.push(DummyStruct(3));
    REQUIRE(DummyStruct::constructedNum == 2);
    REQUIRE(ar.size() == 2);
    REQUIRE(ar[0].value == 5);
    REQUIRE(ar[1].value == 3);

    REQUIRE(ar.pop().value == 3);
    REQUIRE(DummyStruct::constructedNum == 1);
    REQUIRE(ar.size() == 1);

    REQUIRE(ar.pop().value == 5);
    REQUIRE(DummyStruct::constructedNum == 0);
    REQUIRE(ar.size() == 0);
}

TEST_CASE("PushAll", "[array]") {
    Array<int> ar1{1, 2, 3};
    Array<int> ar2{4, 5, 6, 7};
    ar1.pushAll(ar2.cbegin(), ar2.cend());
    REQUIRE(ar1.size() == 7);
    for (int i=0; i < 7; ++i) {
        REQUIRE(ar1[i] == i+1);
    }
}

TEST_CASE("Remove by index", "[array]") {
    Array<int> ar{1, 5, 3, 6, 2, 3};
    ar.remove(0);
    REQUIRE(ar == Array<int>({5, 3, 6, 2, 3}));
    ar.remove(ar.size() - 1);
    REQUIRE(ar == Array<int>({5, 3, 6, 2}));
    ar.remove(2);
    REQUIRE(ar == Array<int>({5, 3, 2}));
}

TEST_CASE("References", "[array]") {
    int a, b, c;
    Array<int&> ar{a, b, c};
    ar[0] = 5;
    ar[1] = 3;
    ar[2] = 1;
    REQUIRE(a == 5);
    REQUIRE(b == 3);
    REQUIRE(c == 1);

    for (auto& i : ar) {
        i = 2;
    }
    REQUIRE(a == 2);
    REQUIRE(b == 2);
    REQUIRE(c == 2);

    auto getter = [](){
        return Array<int>{1, 5, 9 };
    };
    ar = getter();
    REQUIRE(a == 1);
    REQUIRE(b == 5);
    REQUIRE(c == 9);

    int d, e, f;
    tieToArray(d, e, f) = makeArray(3, 1, 4);
    REQUIRE(d == 3);
    REQUIRE(e == 1);
    REQUIRE(f == 4);
}


TEST_CASE("Moving", "[static array]") {
    auto f = []() {
        StaticArray<int, 3> ar = { 1, 3, 5 };
        return ar;
    };

    StaticArray<int, 3> ar(f());
    REQUIRE(ar[0] == 1);
    REQUIRE(ar[1] == 3);
    REQUIRE(ar[2] == 5);
}
