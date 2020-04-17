#include "objects/utility/Dynamic.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("Dynamic construct", "[dynamic]") {
    Dynamic value1;
    REQUIRE_FALSE(value1);

    Dynamic value2(5._f);
    REQUIRE(value2);
    REQUIRE(value2.getType() == DynamicId::FLOAT);
    REQUIRE(Float(value2) == 5._f);
    REQUIRE_ASSERT(Size(value2));
    REQUIRE_ASSERT(value2.get<Vector>());

    Dynamic value3(Vector(2.f, 1.f, 4.f));
    REQUIRE(value3.getType() == DynamicId::VECTOR);
    REQUIRE(value3.get<Vector>() == Vector(2.f, 1.f, 4.f));
}

TEST_CASE("Dynamic copy/move", "[dynamic]") {
    Dynamic value1(4._f);
    Dynamic value2;
    value2 = value1;
    REQUIRE(value2);
    REQUIRE(Float(value2) == 4._f);

    Dynamic value3 = std::move(value1);
    REQUIRE(value3);
    REQUIRE(Float(value3) == 4._f);
}

TEST_CASE("Dynamic get", "[dynamic]") {
    Dynamic value1(Size(8));
    REQUIRE(value1.getType() == DynamicId::SIZE);
    REQUIRE(value1.get<Size>() == 8);
    REQUIRE_ASSERT(value1.get<SymmetricTensor>());
    value1.get<Size>() = 2;
    REQUIRE(Size(value1) == 2);

    const Dynamic value2 = Vector(2.f, 1.f, 0.f);
    REQUIRE(value2.getType() == DynamicId::VECTOR);
    REQUIRE(value2.get<Vector>() == Vector(2.f, 1.f, 0.f));
    static_assert(std::is_same<decltype(value2.get<Vector>()), const Vector&>::value, "static test failed");
}

TEST_CASE("Dynamic getScalar", "[dynamic]") {
    Dynamic value1(5._f);
    REQUIRE(value1.getScalar() == 5._f);
    Dynamic value2(Vector(3._f, 4._f, 12._f));
    REQUIRE(value2.getScalar() == approx(13._f));
    Dynamic value3(SymmetricTensor(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f)));
    REQUIRE(value3.getScalar() > 0);
}

TEST_CASE("Dynamic comparison", "[dynamic]") {
    Dynamic value(5._f);
    REQUIRE(value == 5._f);
    REQUIRE_FALSE(value == 4._f);
    REQUIRE_ASSERT(value == Vector(3._f));

    value = Vector(3._f);
    REQUIRE_ASSERT(value == 5._f);
    REQUIRE(value == Vector(3._f));
    REQUIRE_FALSE(value == Vector(3._f, 3._f, 4._f));
}
