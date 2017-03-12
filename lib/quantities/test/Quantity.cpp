#include "quantities/Quantity.h"
#include "catch.hpp"
#include "objects/containers/ArrayUtils.h"

using namespace Sph;

TEST_CASE("Quantity emplace value", "[quantity]") {
    Quantity q;
    REQUIRE(q.size() == 0);
    q.insert<Float, OrderEnum::FIRST>(4._f, 5); // emplace using values and size
    REQUIRE(q.size() == 5);
    REQUIRE(q.getBuffers<Vector>().size() == 0);
    ArrayView<Float> v, dv;
    tie(v, dv) = q.getBuffers<Float>();
    REQUIRE(v.size() == 5);
    REQUIRE(dv.size() == 5);
    REQUIRE(areAllMatching(v, [](Float f) { return f == 4._f; }));
    REQUIRE(areAllMatching(dv, [](Float f) { return f == 0._f; })); // zero derivative

    REQUIRE(q.getValue<Float>().getView() == v);
    REQUIRE(q.getDt<Float>().getView() == dv);
    REQUIRE(q.getValueEnum() == ValueEnum::SCALAR);
    REQUIRE(q.getOrderEnum() == OrderEnum::FIRST);
}

TEST_CASE("Quantity emplace array", "[quantity]") {
    Quantity q;
    Array<Vector> ar = { Vector(1._f), Vector(2._f, 3._f, 1._f), Vector(0._f) };
    q.emplace<Vector, OrderEnum::SECOND>(std::move(ar), Range(1._f, 3._f));

    REQUIRE(q.size() == 3);
    ArrayView<Vector> v, dv, d2v;
    tie(v, dv, d2v) = q.getBuffers<Vector>();
    REQUIRE(v.size() == 3);
    REQUIRE(dv.size() == 3);
    REQUIRE(d2v.size() == 3);
    REQUIRE(v[1] == Vector(2._f, 3._f, 1._f));
    REQUIRE(areAllMatching(dv, [](Vector f) { return f == Vector(0._f); }));
    REQUIRE(areAllMatching(d2v, [](Vector f) { return f == Vector(0._f); }));
    REQUIRE(q.getOrderEnum() == OrderEnum::SECOND);
    REQUIRE(q.getValueEnum() == ValueEnum::VECTOR);
}
