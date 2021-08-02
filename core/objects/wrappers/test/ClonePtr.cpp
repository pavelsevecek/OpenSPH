#include "objects/wrappers/ClonePtr.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("ClonePtr default construct", "[cloneptr]") {
    RecordType::resetStats();
    ClonePtr<RecordType> p1;
    REQUIRE(RecordType::constructedNum == 0);
    REQUIRE(RecordType::destructedNum == 0);
    REQUIRE(p1 == nullptr);
    REQUIRE(!p1);
    REQUIRE_FALSE(p1);
    REQUIRE_SPH_ASSERT(*p1);
    REQUIRE_SPH_ASSERT(p1->value);
    REQUIRE_FALSE(p1.clone());
}

TEST_CASE("ClonePtr ptr construct", "[cloneptr]") {
    RecordType::resetStats();
    {
        ClonePtr<RecordType> p1(alignedNew<RecordType>(5));
        REQUIRE(RecordType::constructedNum == 1);
        REQUIRE(p1);
        REQUIRE(p1 != nullptr);
        REQUIRE_FALSE(!p1);
        REQUIRE(p1->value == 5);
        REQUIRE(p1->wasValueConstructed);
        REQUIRE(p1.clone());
    }
    REQUIRE(RecordType::existingNum() == 0);
}

TEST_CASE("ClonePtr copy construct", "[cloneptr]") {
    RecordType::resetStats();
    {
        ClonePtr<RecordType> p1(alignedNew<RecordType>(4));
        ClonePtr<RecordType> p2(p1);
        REQUIRE(RecordType::constructedNum == 2);
        REQUIRE(p1.get() != p2.get()); // different ptrs
        REQUIRE(p2);
        REQUIRE(p2->value == 4);
        REQUIRE(p2->wasCopyConstructed);
    }
    REQUIRE(RecordType::existingNum() == 0);
}

TEST_CASE("ClonePtr copy construct nullptr", "[cloneptr]") {
    RecordType::resetStats();
    ClonePtr<RecordType> p1;
    ClonePtr<RecordType> p2(p1);
    REQUIRE(!p2);
    REQUIRE(RecordType::constructedNum == 0);
}

TEST_CASE("ClonePtr move construct", "[cloneptr]") {
    RecordType::resetStats();
    {
        ClonePtr<RecordType> p1(alignedNew<RecordType>(6));
        ClonePtr<RecordType> p2(std::move(p1));
        REQUIRE(RecordType::constructedNum == 1);
        REQUIRE(RecordType::existingNum() == 1);
        REQUIRE(p2);
        REQUIRE(p2->value == 6);
        REQUIRE(p2->wasValueConstructed);
        REQUIRE(!p1);
    }
    REQUIRE(RecordType::existingNum() == 0);
}

TEST_CASE("ClonePtr copy assign", "[cloneptr]") {
    RecordType::resetStats();
    ClonePtr<RecordType> p1(alignedNew<RecordType>(3));
    {
        ClonePtr<RecordType> p2(alignedNew<RecordType>(5));
        REQUIRE(RecordType::existingNum() == 2);
        p1 = p2;
        REQUIRE(RecordType::constructedNum == 3);
        REQUIRE(RecordType::existingNum() == 2);
        REQUIRE(p1->value == 5);
        REQUIRE(p2->value == 5);

        p2->value = 4;
        REQUIRE(p1->value == 5); // different objects
    }
}

TEST_CASE("ClonePtr copy assign nullptr", "[cloneptr]") {
    ClonePtr<RecordType> p1(alignedNew<RecordType>(5));
    ClonePtr<RecordType> p2;
    p1 = p2;
    REQUIRE_FALSE(p1);
    REQUIRE_FALSE(p2);
}

TEST_CASE("ClonePtr move assign", "[cloneptr]") {
    RecordType::resetStats();
    {
        ClonePtr<RecordType> p1(alignedNew<RecordType>(3));
        ClonePtr<RecordType> p2(alignedNew<RecordType>(6));
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

namespace {
struct Derived : public Base {
    static bool destroyed;
    ~Derived() {
        destroyed = true;
    }
};

bool Derived::destroyed = false;
} // namespace

TEST_CASE("ClonePtr cast", "[cloneptr]") {
    {
        ClonePtr<Base> p1;
        p1 = ClonePtr<Derived>(alignedNew<Derived>());
        REQUIRE(p1);
        REQUIRE(p1->value == 5);
        REQUIRE_FALSE(Derived::destroyed);
    }
    REQUIRE(Derived::destroyed);

    ClonePtr<Derived> d1(alignedNew<Derived>());
    ClonePtr<Base> p1 = d1; // ok
    ClonePtr<Derived> d2;
    static_assert(!std::is_convertible<decltype(p1), decltype(d2)>::value, "must not be convertible");
}

TEST_CASE("ClonePtr get", "[cloneptr]") {
    ClonePtr<RecordType> p1;
    REQUIRE(p1.get() == nullptr);
    p1 = ClonePtr<RecordType>(alignedNew<RecordType>(5));
    REQUIRE(p1.get());
    REQUIRE(p1.get()->value == 5);
}

TEST_CASE("makeClone", "[cloneptr]") {
    ClonePtr<RecordType> p1 = makeClone<RecordType>(6);
    REQUIRE(p1);
    REQUIRE(p1->value == 6);
    REQUIRE(p1->wasValueConstructed);
}

TEST_CASE("ClonePtr comparison", "[cloneptr]") {
    ClonePtr<RecordType> p1;
    REQUIRE(p1 == nullptr);
    REQUIRE(nullptr == p1);
    REQUIRE_FALSE(p1 != nullptr);
    REQUIRE_FALSE(nullptr != p1);

    p1 = makeClone<RecordType>(5);
    REQUIRE_FALSE(p1 == nullptr);
    REQUIRE_FALSE(nullptr == p1);
    REQUIRE(p1 != nullptr);
    REQUIRE(nullptr != p1);
}

TEST_CASE("ClonePtr convert to AutoPtr", "[cloneptr]") {
    ClonePtr<RecordType> p1 = makeClone<RecordType>(5);
    AutoPtr<RecordType> a1 = p1;
    REQUIRE(a1);
    REQUIRE(a1->value == 5);
    p1->value = 3;
    REQUIRE(a1->value == 5); // really unique value

    AutoPtr<RecordType> a2 = std::move(p1);
    REQUIRE(a2);
    REQUIRE(a2->value == 3);
    REQUIRE_FALSE(p1);
    REQUIRE_FALSE(p1.clone());

    AutoPtr<Base> a3 = makeClone<Derived>();
    REQUIRE(a3);
}
