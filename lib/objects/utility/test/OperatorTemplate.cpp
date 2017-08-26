#include "objects/utility/OperatorTemplate.h"
#include "catch.hpp"
#include "utils/Utils.h"

using namespace Sph;

namespace {
    struct TestStruct : public OperatorTemplate<TestStruct> {
        int value;

        TestStruct(const int value)
            : value(value) {}

        TestStruct& operator+=(const TestStruct& other) {
            value += other.value;
            return *this;
        }
        bool operator==(const TestStruct& other) const {
            return value == other.value;
        }
        TestStruct operator-() const {
            return TestStruct(-value);
        }
    };
}

TEST_CASE("OperatorTemplate sum", "[operator]") {
    TestStruct t1(2), t2(5);

    TestStruct t3 = t1 + t2;
    REQUIRE(t3.value == 7);

    t3 += TestStruct(3);
    REQUIRE(t3.value == 10);
}

TEST_CASE("OperatorTemplate subtract", "[operator]") {
    TestStruct t1(5), t2(3);
    t1 -= t2;
    REQUIRE(t1.value == 2);
    REQUIRE(t2.value == 3);
    t2 = t1 - t2;
    REQUIRE(t1.value == 2);
    REQUIRE(t2.value == -1);
}

TEST_CASE("OperatorTemplate equality", "[operator]") {
    TestStruct t1(7), t2(7), t3(4);
    REQUIRE(t1 == t2);
    REQUIRE_FALSE(t1 == t3);
    REQUIRE(t1 != t3);
    REQUIRE(t3 == t3);
    REQUIRE_FALSE(t3 != t3);
}

namespace {
    struct MultipliableStruct : public OperatorTemplate<MultipliableStruct, int> {
        float value;

        MultipliableStruct(const float value)
            : value(value) {}

        MultipliableStruct& operator*=(const float x) {
            value *= x;
            return *this;
        }
    };
}

TEST_CASE("OperatorTemplate multiply", "[operator]") {
    MultipliableStruct m1(4);
    m1 *= 3;
    REQUIRE(m1.value == 12);
    m1 /= 6;
    REQUIRE(m1.value == 2);

    MultipliableStruct m2 = m1 * 4;
    REQUIRE(m2.value == 8);

    m2 = 6 * m1;
    REQUIRE(m2.value == 12);

    MultipliableStruct m3 = m2 / 2;
    REQUIRE(m3.value == 2);
}
