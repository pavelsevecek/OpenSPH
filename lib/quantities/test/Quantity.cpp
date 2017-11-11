#include "quantities/Quantity.h"
#include "catch.hpp"
#include "objects/utility/ArrayUtils.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Quantity default construct", "[quantity]") {
    Quantity q;
    REQUIRE_ASSERT(q.size());
    REQUIRE_ASSERT(q.getOrderEnum());
    REQUIRE_ASSERT(q.getValueEnum());
    REQUIRE_ASSERT(q.getValue<Float>());
}

TEST_CASE("Quantity value construct", "[quantity]") {
    Quantity q1(OrderEnum::FIRST, 4._f, 3);
    REQUIRE(q1.size() == 3);
    REQUIRE(q1.getValueEnum() == ValueEnum::SCALAR);
    REQUIRE(q1.getOrderEnum() == OrderEnum::FIRST);
    REQUIRE(q1.getValue<Float>() == Array<Float>({ 4._f, 4._f, 4._f }));
    REQUIRE_ASSERT(q1.getValue<Vector>());
    REQUIRE_NOTHROW(q1.getDt<Float>());
    REQUIRE_ASSERT(q1.getD2t<Float>());

    Quantity q2(OrderEnum::SECOND, SymmetricTensor(2._f), 2);
    REQUIRE(q2.size() == 2);
    REQUIRE(q2.getValueEnum() == ValueEnum::SYMMETRIC_TENSOR);
    REQUIRE(q2.getOrderEnum() == OrderEnum::SECOND);
    REQUIRE_NOTHROW(q2.getValue<SymmetricTensor>());
    REQUIRE_NOTHROW(q2.getDt<SymmetricTensor>());
    REQUIRE_NOTHROW(q2.getD2t<SymmetricTensor>());
    REQUIRE_ASSERT(q2.getValue<Vector>());
}

TEST_CASE("Quantity array construct", "[quantity]") {
    Quantity q1(OrderEnum::FIRST, Array<Vector>({ Vector(1._f), Vector(2._f) }));
    REQUIRE(q1.size() == 2);
    REQUIRE(q1.getValueEnum() == ValueEnum::VECTOR);
    REQUIRE(q1.getOrderEnum() == OrderEnum::FIRST);
    REQUIRE_NOTHROW(q1.getValue<Vector>());
    REQUIRE_NOTHROW(q1.getDt<Vector>());
    REQUIRE_ASSERT(q1.getD2t<Vector>());
}

TEST_CASE("Quantity move construct", "[quantity]") {
    Quantity q1(OrderEnum::FIRST, 2._f, 3);
    Quantity q2 = std::move(q1);
    REQUIRE(q2.size() == 3);
    REQUIRE(q2.getValueEnum() == ValueEnum::SCALAR);
    REQUIRE(q2.getOrderEnum() == OrderEnum::FIRST);
    REQUIRE_NOTHROW(q2.getValue<Float>());
    REQUIRE_NOTHROW(q2.getDt<Float>());
    REQUIRE_ASSERT(q2.getD2t<Float>());
}

static Pair<Quantity> makeTestQuantities() {
    Pair<Quantity> qs(EMPTY_ARRAY);
    qs.push(Quantity(OrderEnum::SECOND, 1._f, 1));
    qs[0].getDt<Float>()[0] = 2._f;
    qs[0].getD2t<Float>()[0] = 3._f;
    qs.push(Quantity(OrderEnum::SECOND, 4._f, 1));
    qs[1].getDt<Float>()[0] = 5._f;
    qs[1].getD2t<Float>()[0] = 6._f;
    return qs;
}

static Array<Float> extractAll(Quantity& q) {
    return { q.getValue<Float>()[0], q.getDt<Float>()[0], q.getD2t<Float>()[0] };
}

TEST_CASE("Quantity swap", "[quantity]") {
    Quantity q1(OrderEnum::FIRST, 2._f, 2);
    Quantity q2(OrderEnum::FIRST, Vector(1._f), 2);
    REQUIRE_ASSERT(q1.swap(q2, VisitorEnum::ALL_BUFFERS));

    tie(q1, q2) = makeTestQuantities();
    // sanity check
    REQUIRE(extractAll(q1) == makeArray(1._f, 2._f, 3._f));
    REQUIRE(extractAll(q2) == makeArray(4._f, 5._f, 6._f));
    q1.swap(q2, VisitorEnum::ALL_BUFFERS);
    REQUIRE(extractAll(q1) == makeArray(4._f, 5._f, 6._f));
    REQUIRE(extractAll(q2) == makeArray(1._f, 2._f, 3._f));

    tie(q1, q2) = makeTestQuantities();
    q1.swap(q2, VisitorEnum::ALL_VALUES);
    REQUIRE(extractAll(q1) == makeArray(4._f, 2._f, 3._f));
    REQUIRE(extractAll(q2) == makeArray(1._f, 5._f, 6._f));

    tie(q1, q2) = makeTestQuantities();
    q1.swap(q2, VisitorEnum::HIGHEST_DERIVATIVES);
    REQUIRE(extractAll(q1) == makeArray(1._f, 2._f, 6._f));
    REQUIRE(extractAll(q2) == makeArray(4._f, 5._f, 3._f));

    /// \todo test all meaningful values of VisitorEnum
}

TEST_CASE("Quantity modification", "[quantity]") {
    Quantity q1(OrderEnum::ZERO, makeArray(1._f, 2._f, 3._f));
    REQUIRE(q1.getValue<Float>() == q1.getPhysicalValue<Float>());
    REQUIRE_ASSERT(q1.getPhysicalValue<Vector>());
    q1.getValue<Float>()[0] = 4._f;
    REQUIRE(q1.getPhysicalValue<Float>() == makeArray(4._f, 2._f, 3._f));
    ArrayView<Float> v, pv;
    // create modification, physical value is now a separate buffer
    tie(v, pv) = q1.modify<Float>();
    REQUIRE(q1.getPhysicalValue<Float>() == makeArray(4._f, 2._f, 3._f));
    v[0] = 8._f;
    pv[0] = -1._f;
    REQUIRE(q1.getValue<Float>() == makeArray(8._f, 2._f, 3._f));
    REQUIRE(q1.getPhysicalValue<Float>() == makeArray(-1._f, 2._f, 3._f));
}

TEST_CASE("Quantity clamp", "[quantity]") {
    Quantity q1(OrderEnum::FIRST, makeArray(0._f, 2._f, 5._f));
    q1.getDt<Float>() = makeArray(8._f, 1._f, -3._f);
    q1.clamp(IndexSequence(0, 3), Interval(1._f, 3._f));
    REQUIRE(q1.getValue<Float>() == makeArray(1._f, 2._f, 3._f));
    REQUIRE(q1.getDt<Float>() == makeArray(8._f, 1._f, -3._f));
}

TEST_CASE("Quantity clone", "[quantity]") {
    Quantity q1(OrderEnum::FIRST, makeArray(0._f, 1._f, 2._f));
    q1.getDt<Float>() = Array<Float>({ 3._f, 4._f, 5._f });

    Quantity q2 = q1.clone(VisitorEnum::HIGHEST_DERIVATIVES);
    REQUIRE(q2.getValueEnum() == ValueEnum::SCALAR);
    REQUIRE(q2.getOrderEnum() == OrderEnum::FIRST);
    REQUIRE(q2.size() == q1.size());
    REQUIRE(q2.getDt<Float>() == Array<Float>({ 3._f, 4._f, 5._f }));
}
