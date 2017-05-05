#include "objects/wrappers/AutoPtr.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("AutoPtr default construct", "[autoptr]") {
    RecordType::resetStats();
    AutoPtr<RecordType> p1;
    REQUIRE(RecordType::constructedNum == 0);
    REQUIRE(RecordType::destructedNum == 0);
    REQUIRE(p1 == nullptr);
    REQUIRE(!p1);
    REQUIRE_FALSE(p1);
    REQUIRE_ASSERT(*p1);
    REQUIRE_ASSERT(p1->value);
}

TEST_CASE("AutoPtr ptr construct", "[autoptr]") {
    RecordType::resetStats();
    {
        AutoPtr<RecordType> p1(new RecordType(5));
        REQUIRE(RecordType::constructedNum == 1);
        REQUIRE(p1);
        REQUIRE(p1 != nullptr);
        REQUIRE_FALSE(!p1);
        REQUIRE(p1->value == 5);
        REQUIRE(p1->wasValueConstructed);
    }
    REQUIRE(RecordType::existingNum() == 0);
}

TEST_CASE("AutoPtr move construct", "[autoptr]") {
    RecordType::resetStats();
    {
        AutoPtr<RecordType> p1(new RecordType(6));
        AutoPtr<RecordType> p2(std::move(p1));
        REQUIRE(RecordType::constructedNum == 1);
        REQUIRE(RecordType::existingNum() == 1);
        REQUIRE(p2);
        REQUIRE(p2->value == 6);
        REQUIRE(p2->wasValueConstructed);
        REQUIRE(!p1);
    }
    REQUIRE(RecordType::existingNum() == 0);
}

TEST_CASE("AutoPtr move assign", "[autoptr]") {
    RecordType::resetStats();
    {
        AutoPtr<RecordType> p1(new RecordType(3));
        AutoPtr<RecordType> p2(new RecordType(6));
        REQUIRE(RecordType::constructedNum == 2);
        REQUIRE(RecordType::existingNum() == 2);
        p2 = std::move(p1);
        REQUIRE(p2);
        REQUIRE(p2->value == 3);
        REQUIRE(p2->wasValueConstructed);
        REQUIRE(RecordType::destructedNum == 1);
        REQUIRE(!p1);
    }
    REQUIRE(RecordType::existingNum() == 0);
}

struct Base : public Polymorphic {
    int value = 5;
};

struct Derived : public Base {
    static bool destroyed;
    ~Derived() {
        destroyed = true;
    }
};

bool Derived::destroyed = false;

TEST_CASE("AutoPtr cast", "[autoptr]") {
    {
        AutoPtr<Base> p1;
        p1 = AutoPtr<Derived>(new Derived());
        REQUIRE(p1);
        REQUIRE(p1->value == 5);
        REQUIRE_FALSE(Derived::destroyed);
    }
    REQUIRE(Derived::destroyed);
}

TEST_CASE("AutoPtr get", "[autoptr]") {
    AutoPtr<RecordType> p1;
    REQUIRE(p1.get() == nullptr);
    p1 = AutoPtr<RecordType>(new RecordType(5));
    REQUIRE(p1.get());
    REQUIRE(p1.get()->value == 5);
}

TEST_CASE("makeAuto", "[autoptr]") {
    AutoPtr<RecordType> p1 = makeAuto<RecordType>(6);
    REQUIRE(p1);
    REQUIRE(p1->value == 6);
    REQUIRE(p1->wasValueConstructed);
}

TEST_CASE("AutoPtr comparison", "[autoptr]") {
    AutoPtr<RecordType> p1;
    REQUIRE(p1 == nullptr);
    REQUIRE(nullptr == p1);
    REQUIRE_FALSE(p1 != nullptr);
    REQUIRE_FALSE(nullptr != p1);

    p1 = makeAuto<RecordType>(5);
    REQUIRE_FALSE(p1 == nullptr);
    REQUIRE_FALSE(nullptr == p1);
    REQUIRE(p1 != nullptr);
    REQUIRE(nullptr != p1);
}
