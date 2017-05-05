#include "objects/wrappers/SharedPtr.h
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("SharedPtr default construct", "[sharedptr]") {
    RecordType::resetStats();
    SharedPtr<RecordType> s1;
    REQUIRE(!s1);
    REQUIRE_FALSE(s1);
    REQUIRE_ASSERT(s1->value);
    REQUIRE_ASSERT(*s1);
    REQUIRE(s1.get() == nullptr);
    REQUIRE(RecordType::constructedNum == 0);
    REQUIRE(RecordType::destructedNum == 0);
}

TEST_CASE("SharedPtr ptr construct", "[sharedptr]") {
    RecordType::resetStats();
    {
        SharedPtr<RecordType> s1(new RecordType(5));
        REQUIRE(s1);
        REQUIRE_FALSE(!s1);
        REQUIRE(s1->value == 5);
        REQUIRE(s1->wasValueConstructed);
        REQUIRE((*s1).value == 5);
        REQUIRE(RecordType::constructedNum == 1);
        REQUIRE(RecordType::destructedNum == 0);
    }
    REQUIRE(RecordType::destructedNum == 1)
}

TEST_CASE("SharedPtr copy construct", "[sharedptr]") {
    RecordType::resetStats();
    {
        SharedPtr<RecordType> s1(new RecordType(6));
        {
            SharedPtr<RecordType> s2(s1);
            REQUIRE(s2);
            REQUIRE(s2->value == 6);
            REQUIRE(s2->wasValueConstructed);
            REQUIRE(RecordType::constructedNum == 1);
            REQUIRE(RecordType::destructedNum == 0);
            REQUIRE(s1.get() == s2.get());
        }
        REQUIRE(RecordType::destructedNum == 0);
        REQUIRE(s1->value == 6);
    }
    REQUIRE(RecordType::destructedNum == 1);
}

TEST_CASE("SharedPtr move construct", "[sharedptr]") {
    RecordType::resetStats();
    {
        SharedPtr<RecordType> s1(new RecordType(7));
        {
            SharedPtr<RecordType> s2(std::move(s1));
            REQUIRE(s2);
            REQUIRE(s2->value == 7);
            REQUIRE(s2->wasValueConstructed);
            REQUIRE(RecordType::constructedNum == 1);
            REQUIRE(RecordType::destructedNum == 0);
            REQUIRE_FALSE(s1);
            REQUIRE(!s1);
        }
        REQUIRE(RecordType::destructedNum == 1);
    }
    REQUIRE(RecordType::destructedNum == 1);
}

TEST_CASE("SharedPtr move assign", "[sharedptr]") {
    RecordType::resetStats();
    {
        SharedPtr<RecordType> s1(new RecordType(2));
        {
            SharedPtr<RecordType> s2;
            s2 = std::move(s1);
            REQUIRE(RecordType::constructedNum == 1);
            REQUIRE(s2->value == 2);
            REQUIRE(!s1);
        }
    }
    REQUIRE(RecordType::destructedNum == 1);

    RecordType::resetStats();
    {
        SharedPtr<RecordType> s3;
        {
            s3 = SharedPtr<RecordType>(new RecordType(8));
            REQUIRE(s3->value == 8);
            REQUIRE(RecordType::constructedNum == 1);
            REQUIRE(RecordType::destructedNum == 0);
        }
        REQUIRE(RecordType::destructedNum == 0);
    }
    REQUIRE(RecordType::destructedNum == 1);
}

TEST_CASE("SharedPtr assign nullptr", "[sharedptr]") {
    RecordType::resetStats();
    SharedPtr<RecordType> s1(new RecordType(1));
    SharedPtr<RecordType> s2 = s1;
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(RecordType::destructedNum == 0);
    s1 = nullptr;
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(RecordType::destructedNum == 0);
    s2 = nullptr;
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(RecordType::destructedNum == 1);
}

TEST_CASE("SharedPtr reset", "[sharedptr]") {
    RecordType::resetStats();
    SharedPtr<RecordType> s1(new RecordType(2));
    s1.reset();
    REQUIRE(RecordType::destructedNum == 1);
    REQUIRE(s1 == nullptr);
    REQUIRE(!s1);
}

TEST_CASE("makeShared", "[sharedptr]") {
    SharedPtr<RecordType> s1 = makeShared<RecordType>(5);
    REQUIRE(s1->wasValueConstructed);
    SharedPtr<RecordType> s2 = makeShared<RecordType>();
    REQUIRE(s2->wasDefaultConstructed);
    SharedPtr<RecordType> s3 = makeShared<RecordType>(*s1);
    REQUIRE(s3->wasCopyConstructed);
    REQUIRE(s3->value == 5);
}

TEST_CASE("WeakPtr nullptr construct", "[sharedptr]") {
    WeakPtr<RecordType> w1;
    REQUIRE_FALSE(w1.lock());

    WeakPtr<RecordType> w2(w1);
    REQUIRE_FALSE(w1.lock());
    REQUIRE_FALSE(w2.lock());
}

TEST_CASE("WeakPtr construct from SharedPtr", "[sharedptr]") {
    RecordType::resetStats();
    SharedPtr<RecordType> s1 = makeShared<RecordType>(6);
    WeakPtr<RecordType> w1(s1);
    REQUIRE(w1.lock());
    SharedPtr<RecordType> s2 = w1.lock();
    s1.reset();
    REQUIRE(s2);
    REQUIRE(s2->value == 6);
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(RecordType::destructedNum == 0);
    s2.reset();
    REQUIRE(!s2);
    REQUIRE(RecordType::destructedNum == 1);
}

TEST_CASE("WeakPtr assign SharedPtr", "[sharedptr]") {
    RecordType::resetStats();
    WeakPtr<RecordType> w1;
    {
        SharedPtr<RecordType> s1 = makeShared<RecordType>(5);
        w1 = s1;
        REQUIRE(w1.lock());
        REQUIRE(w1.lock()->value == 5);
    }
    REQUIRE_FALSE(w1.lock());
}
