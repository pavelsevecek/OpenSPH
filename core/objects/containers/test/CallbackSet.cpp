#include "objects/containers/CallbackSet.h"
#include "catch.hpp"

using namespace Sph;

struct Called {};

TEST_CASE("CallbackSet call using SharedToken", "[callbackset]") {
    CallbackSet<void(int)> set;
    set.insert(nullptr, [](int) { throw Called(); });
    REQUIRE(set.size() == 0);
    REQUIRE(set.empty());
    REQUIRE_NOTHROW(set(5));

    SharedToken token;
    set.insert(token, [](int) { throw Called(); });
    REQUIRE(set.size() == 1);
    REQUIRE_THROWS_AS(set(5), Called);
}

TEST_CASE("CallbackSet call using SharedPtr", "[callbackset]") {
    CallbackSet<void(int)> set;
    set.insert(SharedPtr<int>{}, [](int) { throw Called(); });
    REQUIRE(set.size() == 0);
    REQUIRE(set.empty());
    REQUIRE_NOTHROW(set(5));

    SharedPtr<int> ptr = makeShared<int>();
    int actual = -1;
    set.insert(ptr, [&actual](int value) { actual = value; });
    REQUIRE(set.size() == 1);
    set(5);
    REQUIRE(actual == 5);
}

TEST_CASE("CallbackSet expiration", "[callbackset]") {
    CallbackSet<void(int)> set;
    SharedToken t1;
    int expected = -1;
    bool c1 = false;
    bool c2 = false;
    bool c3 = false;
    {
        SharedToken t2;
        {
            SharedToken t3;

            set.insert(t1, [&](int actual) {
                REQUIRE(actual == expected);
                c1 = true;
            });
            set.insert(t2, [&](int actual) {
                REQUIRE(actual == expected);
                c2 = true;
            });
            set.insert(t3, [&](int actual) {
                REQUIRE(actual == expected);
                c3 = true;
            });
            expected = 5;
            set(expected);
            REQUIRE(bool(c1 && c2 && c3));
        }

        c1 = c2 = c3 = false;
        expected = 3;
        set(expected);
        REQUIRE(bool(c1 && c2 && !c3));
    }

    c1 = c2 = c3 = false;
    expected = 2;
    set(expected);
    REQUIRE(bool(c1 && !c2 && !c3));
}

TEST_CASE("CallbackSet pointer semantics", "[callbackset]") {
    CallbackSet<void(int)> set1;
    CallbackSet<void(int)> set2 = set1;
    SharedToken token;
    int called = 0;
    set1.insert(token, [&called](int) { called++; });
    REQUIRE(set1.size() == 1);
    REQUIRE(set2.size() == 1);

    set1(0);
    set2(1);
    REQUIRE(called == 2);
}
