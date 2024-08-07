#include "objects/containers/StaticArray.h"
#include "catch.hpp"
#include "utils/RecordType.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("StaticArray Construction", "[staticarray]") {
    StaticArray<RecordType, 3> ar1;
    REQUIRE(ar1.maxSize() == 3);
    REQUIRE(ar1.size() == 3);
    REQUIRE_FALSE(ar1.empty());
    for (int i = 0; i < 3; ++i) {
        REQUIRE(ar1[i].wasDefaultConstructed);
    }

    StaticArray<RecordType, 3> ar2(EMPTY_ARRAY);
    REQUIRE(ar2.maxSize() == 3);
    REQUIRE(ar2.size() == 0);
    REQUIRE(ar2.empty());
}

TEST_CASE("StaticArray Construct from initializer list", "[staticarray]") {
    StaticArray<RecordType, 5> ar{ 1, 2, 3 };
    REQUIRE(ar.size() == 3);
    REQUIRE(ar.maxSize() == 5);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(ar[i].wasCopyConstructed); // copies temporary objects created by initializer list
        REQUIRE(ar[i] == RecordType(i + 1));
    }
}

TEST_CASE("StaticArray move construct", "[staticarray]") {
    StaticArray<RecordType, 3> ar1{ 3, 6, 9 };
    StaticArray<RecordType, 3> ar2(std::move(ar1));
    REQUIRE(ar2.size() == 3);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(ar2[i] == RecordType(3 * i + 3));
    }
}

TEST_CASE("StaticArray destructor", "[staticarray]") {
    {
        StaticArray<RecordType, 3> ar{ 0, 1, 2 };
        RecordType::resetStats();
        REQUIRE(RecordType::destructedNum == 0);
    }
    REQUIRE(RecordType::destructedNum == 3);
}

TEST_CASE("StaticArray move assignment") {
    StaticArray<RecordType, 3> ar1;
    {
        StaticArray<RecordType, 3> ar2{ 0, 1 };
        ar1 = std::move(ar2);
    }
    REQUIRE(ar1.size() == 2);
    for (int i = 0; i < 2; ++i) {
        REQUIRE(ar1[i].value == i);
    }
}

TEST_CASE("StaticArray clone", "[staticarray]") {
    StaticArray<RecordType, 4> ar1{ 0, 2, 4 };
    StaticArray<RecordType, 4> ar2;
    ar2 = ar1.clone();
    REQUIRE(ar2.maxSize() == 4);
    REQUIRE(ar2.size() == 3);
    for (int i = 0; i < 3; ++i) {
        REQUIRE(ar2[i].value == 2 * i);
        REQUIRE(ar1[i].value == 2 * i);
        REQUIRE(!ar1[i].wasMoved);
    }
}

TEST_CASE("StaticArray modify", "[staticarray]") {
    StaticArray<RecordType, 4> ar{ 0, 2, 5 };
    ar[0] = RecordType(1);
    RecordType r(3);
    ar[1] = r;
    for (int i = 0; i < 3; ++i) {
        REQUIRE(ar[i].value == 2 * i + 1);
    }
    REQUIRE(ar[0].wasMoveAssigned);
    REQUIRE(ar[1].wasCopyAssigned);
}

TEST_CASE("StaticArray front & back", "[staticarray]") {
    StaticArray<int, 4> ar{ 1, 2, 3, 4 };
    REQUIRE(ar.back() == 4);
    REQUIRE(ar.front() == 1);
    ar.back() = 3;
    REQUIRE(ar.back() == 3);
}

TEST_CASE("StaticArray push & pop", "[staticarray]") {
    StaticArray<RecordType, 4> ar(EMPTY_ARRAY);
    REQUIRE(ar.size() == 0);
    ar.push(RecordType(5));
    REQUIRE(ar.size() == 1);
    REQUIRE(ar[0].value == 5);
    REQUIRE(ar[0].wasMoveConstructed);
    RecordType r(6);
    ar.push(r);
    REQUIRE(ar.size() == 2);
    REQUIRE(ar[1].value == 6);
    REQUIRE(ar[1].wasCopyConstructed);
    ar.push(RecordType(7));
    ar.push(RecordType(8));
    REQUIRE(ar.size() == 4);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(ar[i].value == 5 + i);
    }
    REQUIRE(ar.pop().value == 8);
    REQUIRE(ar.pop().value == 7);
    REQUIRE(ar.pop().value == 6);
    REQUIRE(ar.size() == 1);
    REQUIRE(ar[0].value == 5);
}

TEST_CASE("StaticArray resize", "[staticarray]") {
    RecordType::resetStats();
    StaticArray<RecordType, 4> ar;
    ar.resize(1);
    REQUIRE(RecordType::existingNum() == 1);
    REQUIRE(ar[0].wasDefaultConstructed);
    ar.resize(0);
    REQUIRE(ar.size() == 0);
    REQUIRE(RecordType::existingNum() == 0);

    ar.push(RecordType(1));
    ar.push(RecordType(2));
    ar.resize(4);
    REQUIRE(ar.size() == 4);
    REQUIRE(ar[0] == 1);
    REQUIRE(ar[1] == 2);
    REQUIRE(ar[2].wasDefaultConstructed);
    REQUIRE(ar[3].wasDefaultConstructed);
    ar.resize(1);
    REQUIRE(ar[0] == 1);
    REQUIRE(ar.size() == 1);
}

TEST_CASE("StaticArray fill", "[staticarray]") {
    StaticArray<RecordType, 3> ar;
    ar.fill(RecordType(2));
    for (Size i = 0; i < 3; ++i) {
        REQUIRE(ar[i].value == 2);
        REQUIRE(ar[i].wasCopyAssigned);
    }
}

TEST_CASE("StaticArray references", "[staticarray]") {
    RecordType::resetStats();
    RecordType r1(5), r2(3);
    StaticArray<RecordType&, 4> ar{ r1, r2 };
    REQUIRE(ar[0].value == 5);
    REQUIRE(ar[0].wasValueConstructed);
    REQUIRE(ar[1].value == 3);
    REQUIRE(ar[1].wasValueConstructed);
    ar[0] = RecordType(6);
    REQUIRE(r1.value == 6);
    REQUIRE(r1.wasMoveAssigned);
    r2.value = 3;
    REQUIRE(ar[0].value == 6);
    REQUIRE(ar[1].value == 3);
    ar.~StaticArray<RecordType&, 4>();
    REQUIRE(RecordType::destructedNum == 1); // one temporary
}

TEST_CASE("StaticArray move references", "[staticarray]") {
    RecordType r1(5), r2(10);
    StaticArray<RecordType&, 2> ar1{ r1, r2 };
    RecordType r3, r4;
    tie(r3, r4) = std::move(ar1);
    REQUIRE(!r1.wasMoved);
    REQUIRE(!r2.wasMoved);
    REQUIRE(r3.wasCopyAssigned);
    REQUIRE(r4.wasCopyAssigned);
    REQUIRE(r3.value == 5);
    REQUIRE(r4.value == 10);
}

TEST_CASE("makeStatic", "[staticarray]") {
    auto ar = makeStatic(RecordType(5), RecordType(3));
    REQUIRE(ar.maxSize() == 2);
    REQUIRE(ar.size() == 2);
    REQUIRE(ar[0].value == 5);
    REQUIRE(ar[0].wasCopyConstructed);
    REQUIRE(ar[1].value == 3);
    REQUIRE(ar[1].wasCopyConstructed);
}

TEST_CASE("tieToStatic", "[staticarray]") {
    RecordType r1, r2;
    tie(r1, r2) = makeStatic(RecordType(3), RecordType(6));
    REQUIRE(r1.value == 3);
    REQUIRE(r1.wasMoveAssigned);
    REQUIRE(r2.value == 6);
    REQUIRE(r2.wasMoveAssigned);

    RecordType r3, r4;
    tie(r3, r4) = StaticArray<RecordType&, 5>{ r1, r2 };
    REQUIRE(r3.value == 3);
    REQUIRE(r4.value == 6);
}

TEST_CASE("StaticArray iterate", "[staticarray]") {
    StaticArray<RecordType, 4> ar{ 1, 2, 3, 4 };
    int i = 1;
    for (RecordType& r : ar) {
        REQUIRE(r.value == i);
        r.value = 5;
        i++;
    }
    REQUIRE(ar[0].value == 5);

    RecordType r1, r2;
    int value = 10;
    for (RecordType& r : tie(r1, r2)) {
        r.value = value;
        value += 10;
    }
    REQUIRE(r1.value == 10);
    REQUIRE(r2.value == 20);
}

template <typename T1, typename T2, typename TEnabler = void>
struct AreComparable {
    static constexpr bool value = false;
};
template <typename T1, typename T2>
struct AreComparable<T1, T2, VoidType<decltype(std::declval<const T1>() == std::declval<const T2>())>> {
    static constexpr bool value = true;
};


TEST_CASE("StaticArray equality", "[staticarray]") {
    StaticArray<int, 3> ar1{ 1, 2, 3 };
    StaticArray<int, 3> ar2(EMPTY_ARRAY);
    REQUIRE_FALSE(ar1 == ar2);
    REQUIRE(ar1 != ar2);

    StaticArray<int, 3> ar3{ 1, 2 };
    REQUIRE_FALSE(ar1 == ar3);
    REQUIRE(ar1 != ar3);

    StaticArray<int, 3> ar4{ 1, 2, 4 };
    REQUIRE_FALSE(ar1 == ar4);
    REQUIRE(ar1 != ar4);

    StaticArray<int, 3> ar5{ 1, 2, 3 };
    REQUIRE(ar1 == ar5);
    REQUIRE_FALSE(ar1 != ar5);

    // make sure static arrays of different size are not comparable
    static_assert(AreComparable<StaticArray<int, 4>, StaticArray<int, 4>>::value, "Must be comparable");
    static_assert(!AreComparable<StaticArray<int, 4>, StaticArray<int, 3>>::value, "Must not be comparable");
}
