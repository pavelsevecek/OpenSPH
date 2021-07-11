#include "objects/wrappers/Expected.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Expected default constructor", "[expected]") {
    Expected<RecordType> e;
    REQUIRE(e);
    REQUIRE(e->wasDefaultConstructed);
    REQUIRE_FALSE(!e);
    REQUIRE_SPH_ASSERT(e.error());
}

TEST_CASE("Expected construct expected", "[expected]") {
    Expected<RecordType> e1(RecordType(5));
    REQUIRE(e1);
    REQUIRE(e1->value == 5);
    REQUIRE(e1->wasMoveConstructed);

    Expected<RecordType> e2(e1.value());
    REQUIRE(e2);
    REQUIRE(e2->value == 5);
    REQUIRE(e2->wasCopyConstructed);
}

TEST_CASE("Expected construct unexpected", "[expected]") {
    Expected<RecordType> e(UNEXPECTED, "error");
    REQUIRE_FALSE(e);
    REQUIRE(!e);
    REQUIRE(e.error() == "error");
    REQUIRE_SPH_ASSERT(e->value);
    REQUIRE_SPH_ASSERT(e.value());
}

TEST_CASE("Expected copy/move constuct", "[expected]") {
    Expected<RecordType> e1(5);
    REQUIRE(e1->wasValueConstructed);
    Expected<RecordType> e2(e1);
    REQUIRE(e2);
    REQUIRE(e2->value == 5);
    REQUIRE(e2->wasCopyConstructed);

    Expected<RecordType> e3(std::move(e1));
    REQUIRE(e3);
    REQUIRE(e3->value == 5);
    REQUIRE(e3->wasMoveConstructed);

    RecordType::resetStats();
    Expected<RecordType> e4(UNEXPECTED, "error");
    Expected<RecordType> e5(e4);
    REQUIRE(!e5);
    REQUIRE(e5.error() == "error");
    REQUIRE(RecordType::constructedNum == 0);
}

TEST_CASE("Expected copy/move assign", "[expected]") {
    Expected<RecordType> e1(6);
    Expected<RecordType> e2;
    e2 = e1;
    REQUIRE(e2);
    REQUIRE(e2->wasCopyAssigned);
    REQUIRE(e2->value == 6);

    Expected<RecordType> e3(UNEXPECTED, "error");
    e3 = std::move(e1);
    REQUIRE(e3->wasMoveConstructed);

    e3 = Expected<RecordType>(UNEXPECTED, "err");
    REQUIRE(!e3);
    REQUIRE(e3.error() == "err");
}

TEST_CASE("Expected same types", "[expected]") {
    Expected<std::string> e("test");
    REQUIRE(e);
    REQUIRE(e.value() == "test");
    REQUIRE_SPH_ASSERT(e.error());

    e = Expected<std::string>(UNEXPECTED, "error");
    REQUIRE(!e);
    REQUIRE(e.error() == "error");
}

TEST_CASE("makeUnexpected", "[expected]") {
    Expected<RecordType> e = makeUnexpected<RecordType>("error");
    REQUIRE(!e);
    REQUIRE(e.error() == "error");
}
