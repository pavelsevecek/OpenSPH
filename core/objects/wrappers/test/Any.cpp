#include "objects/wrappers/Any.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Any default constructor", "[any]") {
    Any a;
    REQUIRE_FALSE(a.hasValue());
    REQUIRE_FALSE(anyCast<int>(a));
    REQUIRE_SPH_ASSERT(int(a));
}

TEST_CASE("Any copy constructor of empty", "[any]") {
    Any a1;
    Any a2(a1);
    REQUIRE_FALSE(a2.hasValue());
    REQUIRE_FALSE(a1.hasValue());
}

TEST_CASE("Any move constructor of empty", "[any]") {
    Any a1;
    Any a2(std::move(a1));
    REQUIRE_FALSE(a2.hasValue());
    REQUIRE_FALSE(a1.hasValue());
}

TEST_CASE("Any value constructor", "[any]") {
    Any a1(5);
    REQUIRE(a1.hasValue());
    Optional<float> f = anyCast<float>(a1);
    REQUIRE(!f);
    Optional<int> i = anyCast<int>(a1);
    REQUIRE(i);
    REQUIRE(i.value() == 5);

    Any a2(std::string("hello"));
    REQUIRE(a2.hasValue());
    Optional<std::string> s = anyCast<std::string>(a2);
    REQUIRE(s);
    REQUIRE(s.value() == "hello");
}

TEST_CASE("Any copy constructor", "[any]") {
    Any a1(5);
    Any a2(a1);
    REQUIRE(a2.hasValue());
    Optional<int> i = anyCast<int>(a2);
    REQUIRE(i);
    REQUIRE(i.value() == 5);
    REQUIRE(a1.hasValue());
}

TEST_CASE("Any move constructor", "[any]") {
    Any a1(4);
    Any a2(std::move(a1));
    REQUIRE(a2.hasValue());
    REQUIRE_FALSE(a1.hasValue());

    Optional<int> i = anyCast<int>(a2);
    REQUIRE(i);
    REQUIRE(i.value() == 4);
}

TEST_CASE("Any copy operator", "[any]") {
    Any a1;
    a1 = 5;
    REQUIRE(a1.hasValue());
    REQUIRE(anyCast<int>(a1).value() == 5);

    a1 = 3.14f;
    REQUIRE(a1.hasValue());
    REQUIRE(anyCast<float>(a1).value() == 3.14f);

    Any a2(5.);
    a1 = a2;
    REQUIRE(a1.hasValue());
    REQUIRE(anyCast<double>(a1).value() == 5.f);

    Any a3;
    a1 = a3;
    REQUIRE_FALSE(a1.hasValue());
}

TEST_CASE("Any move operator", "[any]") {
    Any a1;
    RecordType r(6);
    a1 = std::move(r);
    REQUIRE(a1.hasValue());
    REQUIRE(r.wasMoved);

    Optional<RecordType> r2 = anyCast<RecordType>(r);
    REQUIRE(r2);
    REQUIRE(r2->value == 6);

    Any a2(3);
    a2 = std::move(a1);
    REQUIRE(a2.hasValue());
    REQUIRE(anyCast<RecordType>(a2));

    a2 = Any();
    REQUIRE_FALSE(a2.hasValue());
}

TEST_CASE("Any get reference", "[any]") {
    Any a1(3);
    int& i(a1);
    REQUIRE(i == 3);
    i = 6;
    REQUIRE(anyCast<int>(a1).value() == 6);

    Any a2;
    REQUIRE_SPH_ASSERT(int(a2));
}

TEST_CASE("Any value comparisons", "[any]") {
    Any a1(5.);
    REQUIRE(a1 != 5);   // double != int
    REQUIRE(a1 != 5.f); // double != float
    REQUIRE(a1 != 4);   // 5 != 4
    REQUIRE(a1 == 5.);

    Any a2;
    REQUIRE_FALSE(a2 == 5);
}
