#include "objects/containers/Tuple.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "utils/RecordType.h"

using namespace Sph;

TEST_CASE("Tuple default construction", "[tuple]") {
    Tuple<> empty;
    static_assert(empty.size() == 0, "Static test failed");

    Tuple<int, float> t1;
    static_assert(t1.size() == 2, "Static test failed");

    Tuple<RecordType, RecordType> t2;
    REQUIRE(t2.get<0>().value == -1);
    REQUIRE(t2.get<0>().wasDefaultConstructed);
    REQUIRE(t2.get<1>().value == -1);
    REQUIRE(t2.get<1>().wasDefaultConstructed);
}

TEST_CASE("Tuple copy/move construction", "[tuple]") {
    Tuple<RecordType, RecordType> t1(5, 10);
    REQUIRE(t1.get<0>().wasValueConstructed);
    REQUIRE(t1.get<1>().wasValueConstructed);
    Tuple<RecordType, RecordType> t2(t1);
    REQUIRE(t2.get<0>().value == 5);
    REQUIRE(t2.get<0>().wasCopyConstructed);
    REQUIRE(t2.get<1>().value == 10);
    REQUIRE(t2.get<1>().wasCopyConstructed);

    Tuple<RecordType, RecordType> t3(std::move(t1));
    REQUIRE(t3.get<0>().value == 5);
    REQUIRE(!t3.get<0>().wasCopyConstructed);
    REQUIRE(t3.get<0>().wasMoveConstructed);
    REQUIRE(t3.get<1>().value == 10);
    REQUIRE(t3.get<1>().wasMoveConstructed);

    Tuple<int, float> t4{ 3, 4.5_f };
    REQUIRE(t4.get<0>() == 3);
    REQUIRE(t4.get<1>() == 4.5_f);
}

TEST_CASE("Tuple copy/move assignment", "[tuple]") {
    RecordType r1(5), r2(7);
    Tuple<RecordType, RecordType&, RecordType&&> t1(r1, r2, RecordType(9));
    RecordType r3;
    Tuple<RecordType, RecordType&, RecordType> t2(RecordType(1), r3, RecordType(1));
    t2 = t1;
    REQUIRE(t2.size() == 3);
    REQUIRE(t2.get<0>().value == 5);
    REQUIRE(t2.get<0>().wasCopyAssigned);
    REQUIRE(!t1.get<0>().wasMoved);

    REQUIRE(t2.get<1>().value == 7);
    REQUIRE(t2.get<1>().wasCopyAssigned);
    REQUIRE(r3.value == 7);
    REQUIRE(!t2.get<1>().wasMoveAssigned);
    REQUIRE(!t1.get<1>().wasMoved);

    REQUIRE(t2.get<2>().value == 9);
    REQUIRE(!t2.get<2>().wasCopyAssigned);
    REQUIRE(t2.get<2>().wasMoveAssigned);
    REQUIRE(t1.get<2>().wasMoved);

    // test that we cannot move out of const tuple
    const Tuple<RecordType, RecordType&, RecordType&&> t3(RecordType(2), r2, RecordType(3));
    Tuple<RecordType, RecordType, RecordType> t4;
    t4 = t3;
    REQUIRE(t4.get<0>().value == 2);
    REQUIRE(t4.get<0>().wasCopyAssigned);
    REQUIRE(!t4.get<0>().wasMoveAssigned);

    REQUIRE(t4.get<1>().value == 7);
    REQUIRE(t4.get<1>().wasCopyAssigned);
    REQUIRE(!t4.get<1>().wasMoveAssigned);

    REQUIRE(t4.get<2>().value == 3);
    REQUIRE(t4.get<2>().wasCopyAssigned);
    REQUIRE(!t4.get<2>().wasMoveAssigned);
}

TEST_CASE("Const tuple", "[tuple]") {
    RecordType r1(3);
    Tuple<const RecordType, const RecordType&> constTuple(RecordType(5), r1);
    REQUIRE(constTuple.get<0>().value == 5);
    REQUIRE(constTuple.get<0>().wasMoveConstructed);
    REQUIRE(constTuple.get<1>().value == 3);
    REQUIRE(!constTuple.get<1>().wasMoveConstructed);
    REQUIRE(!constTuple.get<1>().wasCopyConstructed);
}

TEST_CASE("Tuple lvalue references", "[tuple]") {
    RecordType r1, r2;
    Tuple<RecordType&, RecordType&> t1(r1, r2);
    t1 = makeTuple(RecordType(5), RecordType(10));
    REQUIRE(r1.wasMoveAssigned);
    REQUIRE(r2.wasMoveAssigned);
    REQUIRE(r1.value == 5);
    REQUIRE(r2.value == 10);

    RecordType r3, r4;
    Tuple<RecordType&, RecordType&> t2(r3, r4);
    t2 = t1;
    REQUIRE(r3.wasCopyAssigned);
    REQUIRE(r4.wasCopyAssigned);
    REQUIRE(r3.value == 5);
    REQUIRE(r4.value == 10);
}

TEST_CASE("Tuple rvalue references", "[tuple]") {
    RecordType record(5);
    Tuple<RecordType&&> t1(std::move(record));
}

TEST_CASE("Get rvalue reference of tuple", "[tuple]") {
    RecordType r1(5), r2(10), r3(15);
    Tuple<RecordType, RecordType&, RecordType&&> t(r1, r2, std::move(r3));
    RecordType r4 = std::move(t).get<0>();
    RecordType r5 = std::move(t).get<1>();
    RecordType r6 = std::move(t).get<2>();
    REQUIRE(r4.value == 5);
    REQUIRE(r4.wasMoveConstructed);
    REQUIRE(r5.value == 10);
    REQUIRE(!r5.wasMoveConstructed); // l-value reference does not get moved
    REQUIRE(!r2.wasMoved);
    REQUIRE(r6.value == 15);
    REQUIRE(r6.wasMoveConstructed);
    REQUIRE(r3.wasMoved);
}

TEST_CASE("Tuple get by type", "[tuple]") {
    RecordType r1(5), r2(10), r3(15);
    Tuple<RecordType, RecordType&, RecordType&&, int> t(r1, r2, std::move(r3), 20);
    decltype(auto) value = t.get<RecordType>();
    static_assert(std::is_same<decltype(value), RecordType&>::value, "static test failed");
    REQUIRE(value.value == 5);
    decltype(auto) lref = t.get<RecordType&>();
    static_assert(std::is_same<decltype(lref), RecordType&>::value, "static test failed");
    REQUIRE(lref.value == 10);
    decltype(auto) rref = t.get<RecordType&&>();
    static_assert(std::is_same<decltype(rref), RecordType&&>::value, "static test failed");
    REQUIRE(rref.value == 15);
    decltype(auto) i = t.get<int>();
    static_assert(std::is_same<decltype(i), int&>::value, "static test failed");
    REQUIRE(i == 20);

    /// \todo check r-value overload
}

TEST_CASE("MakeTuple", "[tuple]") {
    RecordType r1(7);
    auto t1 = makeTuple(r1, RecordType(5));
    REQUIRE(t1.get<0>().value == 7);
    REQUIRE(t1.get<0>().wasCopyConstructed);
    REQUIRE(t1.get<1>().value == 5);
    REQUIRE(t1.get<1>().wasMoveConstructed);
    RecordType r2;
    Tuple<RecordType, RecordType&> t2(RecordType(2), r2);
    REQUIRE(makeTuple(r1, RecordType(12)).get<0>().wasCopyConstructed);
    REQUIRE(makeTuple(r1, RecordType(12)).get<1>().wasMoveConstructed);
    t2 = makeTuple(r1, RecordType(12)); // assign r-value reference tuple
    REQUIRE(t2.get<0>().value == 7);
    REQUIRE(t2.get<0>().wasMoveAssigned);
    REQUIRE(t2.get<1>().value == 12);
    REQUIRE(t2.get<1>().wasMoveAssigned);
}

TEST_CASE("TieToTuple", "[tuple]") {
    RecordType r1(10), r2(20);
    tieToTuple(r1, r2) = makeTuple(r2, r1); // swap values
    REQUIRE(r1.value == 20);
    REQUIRE(r1.wasMoveAssigned);
    REQUIRE(r2.value == 10);
    REQUIRE(r2.wasMoveAssigned);

    RecordType r3(30);
    tieToTuple(r1, r2, IGNORE, r3) = makeTuple(1, 2, 3, 4);
    REQUIRE(r1.value == 1);
    REQUIRE(r2.value == 2);
    REQUIRE(r3.value == 4);

    RecordType r4(6);
    RecordType r5(7);
    Tuple<RecordType, RecordType&, RecordType&&> t(5, r4, std::move(r5));
    RecordType r6, r7, r8;
    tieToTuple(r6, r7, r8) = t;
    REQUIRE(r6.value == 5);
    REQUIRE(r6.wasCopyAssigned);
    REQUIRE(r7.value == 6);
    REQUIRE(r7.wasCopyAssigned);
    REQUIRE(r8.value == 7);
    REQUIRE(r8.wasMoveAssigned);
}

TEST_CASE("ForwardAsTuple", "[tuple]") {
    RecordType r1(5), r2(10);
    const RecordType r3(15);
    // store r1 as l-value ref, r2 as r-value ref, r3 as const l-value ref
    auto t = forwardAsTuple(r1, std::move(r2), r3);
    REQUIRE(!r1.wasMoved);
    REQUIRE(!r2.wasMoved);
    REQUIRE(!r3.wasMoved);
    using Type0 = decltype(t.get<0>());
    using Type1 = decltype(t.get<1>());
    using Type2 = decltype(t.get<2>());
    static_assert(
        std::is_lvalue_reference<Type0>::value && !std::is_const<std::remove_reference_t<Type0>>::value,
        "static test failed");
    static_assert(
        std::is_rvalue_reference<Type1>::value && !std::is_const<std::remove_reference_t<Type1>>::value,
        "static test failed");
    static_assert(
        std::is_lvalue_reference<Type2>::value && std::is_const<std::remove_reference_t<Type2>>::value,
        "static test failed");

    RecordType r4(42);
    RecordType r5(47);
    const RecordType r6(52);
    Tuple<RecordType, RecordType, RecordType> t2 = forwardAsTuple(r4, std::move(r5), r6);

    REQUIRE(t2.get<0>().value == 42);
    REQUIRE(t2.get<0>().wasCopyConstructed);
    REQUIRE(t2.get<1>().value == 47);
    REQUIRE(t2.get<1>().wasMoveConstructed);
    REQUIRE(t2.get<2>().value == 52);
    REQUIRE(t2.get<2>().wasCopyConstructed);
    REQUIRE(!r4.wasMoved);
    REQUIRE(r5.wasMoved);
    REQUIRE(!r6.wasMoved);

    r4.value = 59;
    RecordType r7(61);
    t2 = forwardAsTuple(r4, RecordType(60), r7);
    REQUIRE(t2.get<0>().value == 59);
    REQUIRE(t2.get<0>().wasCopyAssigned);
    REQUIRE(t2.get<1>().value == 60);
    REQUIRE(t2.get<1>().wasMoveAssigned);
    REQUIRE(t2.get<2>().value == 61);
    REQUIRE(t2.get<2>().wasCopyAssigned);
}

TEST_CASE("ForEach", "[tuple]") {
    Tuple<int, float, double, char> t = makeTuple(1, 2._f, 3., 5);
    int sum = 0;
    forEach(t, [&sum](auto& value) { sum += value; });
    REQUIRE(sum == 11);
}


TEST_CASE("Append Tuple", "[tuple]") {
    RecordType r1(7);
    Tuple<RecordType, RecordType&> t1(RecordType(5), r1);
    REQUIRE(t1.get<0>().wasMoveConstructed);

    RecordType r2(11);
    auto t2 = append(t1, RecordType(9), r2);
    static_assert(t2.size() == 4, "static test failed");
    REQUIRE(t2.get<0>().value == 5);
    REQUIRE(t2.get<1>().value == 7);
    REQUIRE(t2.get<2>().value == 9);
    REQUIRE(t2.get<3>().value == 11);

    REQUIRE(t2.get<0>().wasCopyConstructed);
    REQUIRE(!t2.get<1>().wasCopyConstructed);
    REQUIRE(t2.get<2>().wasMoveConstructed);
    REQUIRE(!t2.get<3>().wasMoveConstructed);
    REQUIRE(!t2.get<3>().wasCopyConstructed);
    t2.get<1>() = 42;
    t2.get<3>() = 43;
    REQUIRE(r1.value == 42);
    REQUIRE(r2.value == 43);

    auto t3 = append(std::move(t2), RecordType(14));
    REQUIRE(t3.get<0>().wasMoveConstructed);
    REQUIRE(!t3.get<1>().wasMoveConstructed);
    REQUIRE(!t3.get<1>().wasCopyConstructed);
    REQUIRE(t3.get<2>().wasMoveConstructed);
    REQUIRE(!t3.get<3>().wasMoveConstructed);
    REQUIRE(t3.get<4>().value == 14);
    REQUIRE(t3.get<4>().wasMoveConstructed);
    t3.get<1>() = 90;
    t3.get<3>() = 91;
    REQUIRE(r1.value == 90);
    REQUIRE(r2.value == 91);
    REQUIRE(t2.get<1>().value == 90);
    REQUIRE(t2.get<3>().value == 91);
}

struct DummyFunctor {
    template <typename T1, typename T2, typename T3>
    void operator()(T1&& param1, T2&& param2, T3&& param3) {
        static_assert(
            std::is_lvalue_reference<T1&&>::value && !std::is_const<std::remove_reference_t<T1>>::value,
            "static test failed");
        static_assert(
            std::is_rvalue_reference<T2&&>::value && !std::is_const<std::remove_reference_t<T2>>::value,
            "static test failed");
        static_assert(
            std::is_lvalue_reference<T3&&>::value && std::is_const<std::remove_reference_t<T3>>::value,
            "static test failed");
        REQUIRE(param1.value == 5);
        REQUIRE(!param1.wasMoveConstructed);
        REQUIRE(!param1.wasCopyConstructed);
        param1.value = 1;

        REQUIRE(param2.value == 10);
        REQUIRE(!param2.wasMoveConstructed);
        REQUIRE(!param2.wasCopyConstructed);

        REQUIRE(param3.value == 15);
        REQUIRE(!param3.wasMoveConstructed);
        REQUIRE(!param3.wasCopyConstructed);

        // use values
        RecordType r1 = std::forward<T1>(param1);
        REQUIRE(r1.wasCopyConstructed);
        RecordType r2 = std::forward<T2>(param2);
        REQUIRE(r2.wasMoveConstructed);
        RecordType r3 = std::forward<T3>(param3);
        REQUIRE(r3.wasCopyConstructed);
    }
};

TEST_CASE("Apply Tuple", "[tuple]") {
    RecordType r1(5);
    RecordType r2(10);
    const RecordType r3(15);
    auto t1 = forwardAsTuple(r1, std::move(r2), r3);
    apply(t1, DummyFunctor());
    REQUIRE(r1.value == 1);
    REQUIRE(!r1.wasMoved);
    REQUIRE(r2.wasMoved);
    REQUIRE(!r3.wasMoved);

    Tuple<RecordType, RecordType&> t2(RecordType(4), r1);
    // copy/move into lambda parameters
    apply(std::move(t2), [](RecordType param1, RecordType param2) {
        REQUIRE(param1.wasMoveConstructed);
        REQUIRE(!param1.wasCopyConstructed);
        REQUIRE(param2.wasCopyConstructed);
        REQUIRE(!param2.wasMoveConstructed);
        REQUIRE(param1.value == 4);
        REQUIRE(param2.value == 1);
    });

    RecordType r4(12);
    Tuple<RecordType, RecordType&, RecordType&&> t3(RecordType(66), r1, std::move(r4));
    apply(t3, [](RecordType param1, RecordType param2, RecordType param3) {
        REQUIRE(param1.wasCopyConstructed);
        REQUIRE(param2.wasCopyConstructed);
        REQUIRE(param3.wasMoveConstructed);
        REQUIRE(param1.value == 66);
        REQUIRE(param2.value == 1);
        REQUIRE(param3.value == 12);
    });
}

TEST_CASE("Tuple comparison", "[tuple]") {
    Tuple<RecordType, RecordType, RecordType> t1(1, 5, 10);
    auto t2 = makeTuple(RecordType(1), RecordType(5), RecordType(10));
    REQUIRE(t1 == t2);
    REQUIRE_FALSE(t1 != t2);
    REQUIRE(t1 != makeTuple(RecordType(0), RecordType(5), RecordType(10)));
    REQUIRE(t1 != makeTuple(RecordType(1), RecordType(4), RecordType(10)));
    REQUIRE(t1 != makeTuple(RecordType(1), RecordType(5), RecordType(9)));
}


TEST_CASE("Tuple contains", "[tuple]") {
    using TestTuple = Tuple<int, char, RecordType>;
    static_assert(TupleContains<TestTuple, int>::value, "static test failed");
    static_assert(TupleContains<TestTuple, char>::value, "static test failed");
    static_assert(TupleContains<TestTuple, RecordType>::value, "static test failed");
    static_assert(!TupleContains<TestTuple, float>::value, "static test failed");
    static_assert(!TupleContains<TestTuple, void>::value, "static test failed");
}
