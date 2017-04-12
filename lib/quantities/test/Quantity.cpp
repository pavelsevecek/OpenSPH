#include "quantities/Quantity.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"

using namespace Sph;

TEST_CASE("Quantity construct", "[quantity]") {
    Quantity q1;
    REQUIRE(q1.size() == 0);

    Quantity q2(OrderEnum::FIRST, 4._f, 5);
    REQUIRE(q2.size() == 5);
    REQUIRE(q2.getAll<Vector>().size() == 0);
    ArrayView<Float> v, dv;
    tie(v, dv) = q2.getAll<Float>();
    REQUIRE(v.size() == 5);
    REQUIRE(dv.size() == 5);
    REQUIRE(areAllMatching(v, [](Float f) { return f == 4._f; }));
    REQUIRE(areAllMatching(dv, [](Float f) { return f == 0._f; })); // zero derivative

    REQUIRE(q2.getValue<Float>().getView() == v);
    REQUIRE(q2.getDt<Float>().getView() == dv);
    REQUIRE(q2.getValueEnum() == ValueEnum::SCALAR);
    REQUIRE(q2.getOrderEnum() == OrderEnum::FIRST);
}

TEST_CASE("Quantity construct with array", "[quantity]") {
    Array<Vector> ar = { Vector(1._f), Vector(2._f, 3._f, 1._f), Vector(0._f) };
    Quantity q(OrderEnum::SECOND, std::move(ar));

    REQUIRE(q.size() == 3);
    ArrayView<Vector> v, dv, d2v;
    tie(v, dv, d2v) = q.getAll<Vector>();
    REQUIRE(v.size() == 3);
    REQUIRE(dv.size() == 3);
    REQUIRE(d2v.size() == 3);
    REQUIRE(v[1] == Vector(2._f, 3._f, 1._f));
    REQUIRE(areAllMatching(dv, [](Vector f) { return f == Vector(0._f); }));
    REQUIRE(areAllMatching(d2v, [](Vector f) { return f == Vector(0._f); }));
    REQUIRE(q.getOrderEnum() == OrderEnum::SECOND);
    REQUIRE(q.getValueEnum() == ValueEnum::VECTOR);
}
