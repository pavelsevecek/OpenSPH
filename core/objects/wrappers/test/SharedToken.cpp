#include "objects/wrappers/SharedToken.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("SharedToken default construct", "[sharedtoken]") {
    SharedToken token;
    REQUIRE(token);
    REQUIRE_FALSE(!token);

    SharedToken t2;
    token = asConst(t2);
    REQUIRE(token);
    REQUIRE_FALSE(!token);
}

TEST_CASE("SharedToken nullptr construct", "[sharedtoken]") {
    SharedToken token(nullptr);
    REQUIRE_FALSE(token);
    REQUIRE(!token);

    SharedToken t2(nullptr);
    token = asConst(t2);
    REQUIRE_FALSE(token);
    REQUIRE(!token);

    token = SharedPtr<int>();
    REQUIRE_FALSE(token);
}

TEST_CASE("SharedToken SharedPtr construct", "[sharedtoken]") {
    RecordType::resetStats();
    {
        SharedPtr<RecordType> ptr = makeShared<RecordType>();
        SharedToken token = ptr;
        REQUIRE(ptr.getUseCount() == 2);
        REQUIRE(RecordType::constructedNum == 1);

        ptr.reset();
        REQUIRE(RecordType::destructedNum == 0);
    }
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(RecordType::destructedNum == 1);
}

TEST_CASE("WeakToken SharedPtr construct", "[sharedtoken]") {
    RecordType::resetStats();

    SharedPtr<RecordType> ptr = makeShared<RecordType>();
    WeakToken token = ptr;
    REQUIRE(ptr.getUseCount() == 1);
    REQUIRE(RecordType::constructedNum == 1);
    REQUIRE(token.lock());

    ptr.reset();
    REQUIRE(RecordType::destructedNum == 1);
    REQUIRE_FALSE(token.lock());
}

TEST_CASE("SharedToken assignment", "[sharedtoken]") {
    RecordType::resetStats();
    {
        SharedToken token;

        SharedPtr<RecordType> ptr = makeShared<RecordType>();
        token = ptr;
        ptr.reset();
        REQUIRE(RecordType::destructedNum == 0);

        token = makeShared<int>(5);
        REQUIRE(RecordType::destructedNum == 1);

        token = makeShared<RecordType>();
        REQUIRE(RecordType::destructedNum == 1);
    }
    REQUIRE(RecordType::destructedNum == 2);
}
