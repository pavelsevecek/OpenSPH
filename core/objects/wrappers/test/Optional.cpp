#include "objects/wrappers/Optional.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "utils/RecordType.h"

using namespace Sph;


TEST_CASE("Optional constructor", "[optional]") {
    RecordType::resetStats();
    Optional<RecordType> o1;
    REQUIRE(!o1);
    REQUIRE(RecordType::constructedNum == 0);

    Optional<RecordType> o2(5);
    REQUIRE(o2);
    REQUIRE(o2->wasValueConstructed);
    REQUIRE(o2->value == 5);

    Optional<RecordType> o3(o2);
    REQUIRE(o3);
    REQUIRE(o3->wasCopyConstructed);
    REQUIRE(o3->value == 5);

    Optional<RecordType> o4(std::move(o2));
    REQUIRE(o4);
    REQUIRE(o4->value == 5);
    REQUIRE(o4->wasMoveConstructed);

    RecordType::resetStats();
    Optional<RecordType> o5(NOTHING);
    REQUIRE(!o5);
    REQUIRE(RecordType::constructedNum == 0);
}

TEST_CASE("Optional assign value", "[optional]") {
    Optional<RecordType> o1;
    RecordType r1(6);
    o1 = r1;
    REQUIRE(o1);
    REQUIRE(o1->wasCopyConstructed);
    REQUIRE(!o1->wasCopyAssigned);
    REQUIRE(o1->value == 6);

    RecordType r2(7);
    o1 = r2;
    REQUIRE(o1->wasCopyAssigned);
    REQUIRE(o1->value == 7);

    Optional<RecordType> o2;
    o2 = std::move(r1);
    REQUIRE(r1.wasMoved);
    REQUIRE(o2->wasMoveConstructed);
    REQUIRE(o2->value == 6);

    o2 = std::move(r2);
    REQUIRE(r2.wasMoved);
    REQUIRE(o2->wasMoveAssigned);
    REQUIRE(o2->value == 7);

    RecordType::resetStats();
    o2 = NOTHING;
    REQUIRE(!o2);
    REQUIRE(RecordType::destructedNum == 1);
    o2 = RecordType(3);
    REQUIRE(o2);
    REQUIRE(o2->value == 3);
}

TEST_CASE("Optional assign optional", "[optional]") {
    Optional<RecordType> o1;
    Optional<RecordType> o2;
    o1 = o2;
    REQUIRE(!o1);
    REQUIRE(!o2);

    Optional<RecordType> o3(7);
    o1 = o3;
    REQUIRE(o1);
    REQUIRE(o1->wasCopyConstructed);
    REQUIRE(o1->value == 7);
    REQUIRE(o3->value == 7);
    REQUIRE(!o3->wasMoved);

    Optional<RecordType> o4(8);
    o1 = o4;
    REQUIRE(o1->wasCopyAssigned);
    REQUIRE(o1->value == 8);

    Optional<RecordType> o5;
    RecordType::resetStats();
    o1 = o5;
    REQUIRE(!o1);
    REQUIRE(RecordType::destructedNum == 1);

    Optional<RecordType> o6;
    o6 = std::move(o4);
    REQUIRE(o6);
    REQUIRE(o6->value == 8);
    REQUIRE(o6->wasMoveConstructed);
    REQUIRE(o4->wasMoved);
    o6 = std::move(o3);
    REQUIRE(o6);
    REQUIRE(o6->value == 7);
    REQUIRE(o6->wasMoveAssigned);
    REQUIRE(o3->wasMoved);
}

TEST_CASE("Optional emplace", "[optional]") {
    Optional<RecordType> o1;
    o1.emplace(2);
    REQUIRE(o1);
    REQUIRE(o1->wasValueConstructed);
    REQUIRE(o1->value == 2);

    RecordType::resetStats();
    o1.emplace(6);
    REQUIRE(RecordType::destructedNum == 1);
    REQUIRE(o1->value == 6);
}

TEST_CASE("Optional get", "[optional]") {
    Optional<RecordType> o1(5);
    REQUIRE(o1.value().value == 5);
    o1.value() = RecordType(3);
    REQUIRE(o1->value == 3);
}

TEST_CASE("Optional valueOr", "[optional]") {
    Optional<RecordType> o1(4);
    REQUIRE(o1.valueOr(6).value == 4);
    Optional<RecordType> o2;
    REQUIRE(o2.valueOr(3).value == 3);
}

TEST_CASE("Optional valueOrThrow", "[optional]") {
    Optional<RecordType> o1(4);
    REQUIRE(o1.valueOrThrow<std::runtime_error>("test").value == 4);
    Optional<RecordType> o2;
    REQUIRE_THROWS_AS(o2.valueOrThrow<std::runtime_error>("test"), std::runtime_error);
}

TEST_CASE("Optional references", "[optional]") {
    Optional<RecordType&> o1;
    REQUIRE(!o1);
    Optional<RecordType&> o2(NOTHING);
    REQUIRE(!o2);

    RecordType r1(5);
    Optional<RecordType&> o3(r1);
    REQUIRE(o3);
    REQUIRE(o3->value == 5);
    REQUIRE(o3->wasValueConstructed);

    o3->value = 3;
    REQUIRE(r1.value == 3);
    r1.value = 10;
    REQUIRE(o3->value == 10);

    Optional<const RecordType&> o4(r1);
    REQUIRE(o4);
    REQUIRE(o4->value == 10);
    o4 = NOTHING;
    REQUIRE(!o4);
}

TEST_CASE("Optional cast", "[optional]") {
    Optional<int> o1(5);
    Optional<RecordType> o2;
    o2 = optionalCast<RecordType>(o1);
    REQUIRE(o2);
    REQUIRE(o2->wasMoveConstructed); // assigns temporary
    REQUIRE(o2->value == 5);
}
