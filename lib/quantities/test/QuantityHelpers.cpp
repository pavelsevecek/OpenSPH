#include "quantities/QuantityHelpers.h"
#include "catch.hpp"

using namespace Sph;


template <typename TValue>
void executeType(int& a);

template <>
void executeType<Float>(int& a) {
    a = 1;
}
template <>
void executeType<Vector>(int& a) {
    a = 2;
}
template <>
void executeType<Tensor>(int& a) {
    a = 3;
}
template <>
void executeType<SymmetricTensor>(int& a) {
    a = 4;
}
template <>
void executeType<TracelessTensor>(int& a) {
    a = 5;
}
template <>
void executeType<Size>(int& a) {
    a = 6;
}


struct TestVisitor {
    template <typename TValue>
    void visit(int& a) {
        executeType<TValue>(a);
    }
};

TEST_CASE("Dispatch", "[quantityhelpers]") {
    int a;
    dispatchAllTypes(ValueEnum::SCALAR, TestVisitor(), a);
    REQUIRE(a == 1);
    dispatchAllTypes(ValueEnum::VECTOR, TestVisitor(), a);
    REQUIRE(a == 2);
    dispatchAllTypes(ValueEnum::TENSOR, TestVisitor(), a);
    REQUIRE(a == 3);
    dispatchAllTypes(ValueEnum::SYMMETRIC_TENSOR, TestVisitor(), a);
    REQUIRE(a == 4);
    dispatchAllTypes(ValueEnum::TRACELESS_TENSOR, TestVisitor(), a);
    REQUIRE(a == 5);
    dispatchAllTypes(ValueEnum::INDEX, TestVisitor(), a);
    REQUIRE(a == 6);
}
