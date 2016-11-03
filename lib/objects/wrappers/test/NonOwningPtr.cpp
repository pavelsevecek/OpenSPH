#include "objects/wrappers/NonOwningPtr.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;

struct Obs : public Observable {
    int value;
};

struct DerivedObs : public Obs {
    float f;
};

TEST_CASE("NonOwningPtr Construction") {
    NonOwningPtr<Obs> ptr;
    REQUIRE(ptr == nullptr);
    REQUIRE(!ptr);

    {
        Obs owner;
        owner.value = 5;
        ptr         = &owner;
        REQUIRE(!(!ptr));
        REQUIRE(ptr == &owner);
        REQUIRE(ptr->value == 5);
        REQUIRE(owner.getReferenceCnt() == 1);
    }

    REQUIRE(!ptr);
    REQUIRE(ptr == nullptr);
}

TEST_CASE("NonOwningPtr Multiple assignments") {
    NonOwningPtr<Obs> ptr1;
    NonOwningPtr<Obs> ptr2;
    {
        Obs owner;
        ptr1        = &owner;
        owner.value = 5;
        REQUIRE(owner.getReferenceCnt() == 1);

        ptr2 = &owner;
        REQUIRE(owner.getReferenceCnt() == 2);

        {
            NonOwningPtr<Obs> ptr3 = &owner;
            REQUIRE(owner.getReferenceCnt() == 3);
            ptr1 = &owner;
            ptr2 = &owner;
            REQUIRE(owner.getReferenceCnt() == 3);
            REQUIRE(ptr1->value == 5);
            REQUIRE(ptr2->value == 5);
            REQUIRE(ptr3->value == 5);
        }
        REQUIRE(ptr1->value == 5);
        REQUIRE(ptr2->value == 5);
        REQUIRE(owner.getReferenceCnt() == 2);
        ptr1 = nullptr;
        ptr1 = nullptr;
        REQUIRE(owner.getReferenceCnt() == 1);
        REQUIRE(!ptr1);
        REQUIRE(ptr2->value == 5);
    }
    REQUIRE(!ptr1);
    REQUIRE(!ptr2);
}

TEST_CASE("NonOwningPtr Casts") {
    Obs parentOwner;
    DerivedObs derivedOwner;
    NonOwningPtr<DerivedObs> derivedPtr(&derivedOwner);
    // implicit conversion to parent type
    NonOwningPtr<Obs> parentPtr = derivedPtr;

    // dynamic cast to derived type
    static_assert(!std::is_convertible<decltype(parentPtr), decltype(derivedPtr)>::value,
                  "static test failed");
    derivedPtr = nonOwningDynamicCast<DerivedObs>(parentPtr);
}
