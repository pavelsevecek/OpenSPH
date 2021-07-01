#include "objects/containers/ArrayRef.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("ArrayRef default construct", "[arrayref]") {
    RecordType::resetStats();
    ArrayRef<RecordType> ref;
    REQUIRE(ref.empty());
    REQUIRE(ref.size() == 0);
    REQUIRE_FALSE(ref.owns());

    REQUIRE(RecordType::constructedNum == 0);
}

TEST_CASE("ArrayRef weak reference", "[arrayref]") {
    Array<RecordType> holder{ 2, 4 };
    RecordType::resetStats();
    ArrayRef<RecordType> ref(holder, RefEnum::WEAK);
    REQUIRE(RecordType::constructedNum == 0);
    REQUIRE(RecordType::destructedNum == 0);

    REQUIRE(ref.size() == 2);
    REQUIRE_FALSE(ref.empty());
    REQUIRE_FALSE(ref.owns());
    REQUIRE(ref[0].value == 2);
    REQUIRE(ref[1].value == 4);

    holder[0].value = 5;
    REQUIRE(ref[0].value == 5);
    ref[1].value = 7;
    REQUIRE(holder[1].value == 7);

    ref = ArrayRef<RecordType>();
    REQUIRE(RecordType::destructedNum == 0);
}

TEST_CASE("ArrayRef strong reference", "[arrayref]") {
    Array<RecordType> holder{ 3, 6, 9 };
    RecordType::resetStats();
    ArrayRef<RecordType> ref(holder, RefEnum::STRONG);
    REQUIRE(RecordType::constructedNum == 3);
    REQUIRE(RecordType::destructedNum == 0);

    REQUIRE(ref.size() == 3);
    REQUIRE_FALSE(ref.empty());
    REQUIRE(ref.owns());
    REQUIRE(ref[0].value == 3);
    REQUIRE(ref[1].value == 6);
    REQUIRE(ref[2].value == 9);

    holder[0].value = 0;
    REQUIRE(ref[0].value == 3);
    holder = Array<RecordType>();
    REQUIRE(RecordType::destructedNum == 3);
    REQUIRE(ref.size() == 3);
    REQUIRE(ref[0].value == 3);

    ref = ArrayRef<RecordType>();
    REQUIRE(RecordType::destructedNum == 6);
}

TEST_CASE("ArrayRef move weak", "[arrayref]") {
    RecordType::resetStats();
    Array<RecordType> holder{ 0, 1 };
    ArrayRef<RecordType> ref1(holder, RefEnum::WEAK);
    ArrayRef<RecordType> ref2(std::move(ref1));

    REQUIRE(ref2.size() == 2);
    REQUIRE_FALSE(ref2.empty());
    REQUIRE_FALSE(ref2.owns());
    REQUIRE(ref2[0].value == 0);
    REQUIRE_FALSE(ref1.empty());
    REQUIRE_FALSE(ref1.size() == 0);

    holder[0].value = 4;
    REQUIRE(ref2[0].value == 4);
    ref2[1].value = 5;
    REQUIRE(holder[1].value == 5);

    REQUIRE_FALSE(ref1[0].wasMoved);
    REQUIRE_FALSE(ref1[1].wasMoved);
    REQUIRE_FALSE(ref2[0].wasMoveConstructed);
    REQUIRE_FALSE(ref2[1].wasMoveConstructed);
}

TEST_CASE("ArrayRef move strong", "[arrayref]") {
    RecordType::resetStats();
    Array<RecordType> holder{ 2, 3 };
    ArrayRef<RecordType> ref1(holder, RefEnum::STRONG);
    ArrayRef<RecordType> ref2(std::move(ref1));

    REQUIRE(ref2.size() == 2);
    REQUIRE_FALSE(ref2.empty());
    REQUIRE(ref2.owns());
    REQUIRE(ref2[0].value == 2);
    REQUIRE(ref1.empty());
    REQUIRE(ref1.size() == 0);
    REQUIRE_FALSE(ref1.owns());

    holder[0].value = 4;
    REQUIRE(ref2[0].value == 2);
    ref2[1].value = 5;
    REQUIRE(holder[1].value == 3);
}

TEST_CASE("ArrayRef seize", "[arrayref]") {
    Array<RecordType> holder{ 5, 4 };
    ArrayRef<RecordType> ref(holder, RefEnum::WEAK);
    REQUIRE_FALSE(ref.owns());

    RecordType::resetStats();
    ref.seize();
    REQUIRE(ref.owns());
    REQUIRE(ref.size() == 2);
    REQUIRE_FALSE(ref.empty());
    REQUIRE(ref[0].value == 5);
    REQUIRE(ref[1].value == 4);
    REQUIRE(ref[0].wasCopyConstructed);
    REQUIRE(ref[1].wasCopyConstructed);

    ref[0].value = 1;
    REQUIRE(holder[0].value == 5);
    holder[1].value = 2;
    REQUIRE(ref[1].value == 4);

    REQUIRE(RecordType::constructedNum == 2);
    REQUIRE(RecordType::destructedNum == 0);

    ArrayRef<RecordType> empty;
    empty.seize();
    REQUIRE_FALSE(empty.owns());
}

TEST_CASE("ArrayRef const", "[arrayref]") {
    Array<RecordType> holder{ 3, 4 };
    ArrayRef<const RecordType> ref(holder, RefEnum::WEAK);

    // checks that we cannot assign to const ArrayRef
    static_assert(
        !std::is_assignable<decltype(ref[0]), RecordType>::value, "Const ArrayRef shall not be modifyable");

    REQUIRE(ref[0].value == 3);
    REQUIRE(ref[1].value == 4);
}
