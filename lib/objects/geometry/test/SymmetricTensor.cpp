#include "objects/geometry/SymmetricTensor.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("SymmetricTensor construction", "[symmetrictensor]") {
    REQUIRE_NOTHROW(SymmetricTensor t1);

    SymmetricTensor t2(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    REQUIRE(t2[0] == Vector(1._f, -1._f, -2._f));
    REQUIRE(t2[1] == Vector(-1._f, 2._f, -3._f));
    REQUIRE(t2[2] == Vector(-2._f, -3._f, 3._f));

    REQUIRE(t2(0, 0) == 1._f);
    REQUIRE(t2(0, 1) == -1._f);
    REQUIRE(t2(0, 2) == -2._f);
    REQUIRE(t2(1, 0) == -1._f);
    REQUIRE(t2(1, 1) == 2._f);
    REQUIRE(t2(1, 2) == -3._f);
    REQUIRE(t2(2, 0) == -2._f);
    REQUIRE(t2(2, 1) == -3._f);
    REQUIRE(t2(2, 2) == 3._f);

    SymmetricTensor t3(Vector(1._f, -1._f, -2._f), Vector(-1._f, 2._f, -3._f), Vector(-2._f, -3._f, 3._f));
    REQUIRE(t2 == t3);
}

TEST_CASE("SymmetricTensor operations", "[symmetrictensor]") {
    SymmetricTensor t1(Vector(2._f, 1._f, -1._f), Vector(2._f, 3._f, -4._f));
    SymmetricTensor t2(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    REQUIRE(t1 + t2 == SymmetricTensor(Vector(3._f, 3._f, 2._f), Vector(1._f, 1._f, -7._f)));
    REQUIRE(t1 - t2 == SymmetricTensor(Vector(1._f, -1._f, -4._f), Vector(3._f, 5._f, -1._f)));
    SymmetricTensor t3 = SymmetricTensor::null();
    t3 += t1;
    REQUIRE(t3 == t1);
    t3 -= t2;
    REQUIRE(t3 == t1 - t2);
    REQUIRE(3._f * t1 == SymmetricTensor(Vector(6._f, 3._f, -3._f), Vector(6._f, 9._f, -12._f)));
    REQUIRE(3._f * t1 == t1 * 3._f);

    REQUIRE(t1 / 2._f == SymmetricTensor(Vector(1._f, 0.5_f, -0.5_f), Vector(1._f, 1.5_f, -2._f)));

    REQUIRE(t1 * t2 == SymmetricTensor(Vector(2._f, 2._f, -3._f), Vector(-2._f, -6._f, 12._f)));
    REQUIRE(t1 / t2 ==
            approx(SymmetricTensor(Vector(2._f, 0.5_f, -1._f / 3._f), Vector(-2._f, -1.5_f, 4._f / 3._f))));

    REQUIRE(-t1 == SymmetricTensor(Vector(-2._f, -1._f, 1._f), Vector(-2._f, -3._f, 4._f)));
}

TEST_CASE("SymmetricTensor apply", "[symmetrictensor]") {
    SymmetricTensor t(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    Vector v(2._f, 1._f, -1._f);

    REQUIRE(t * v == Vector(3._f, 3._f, -10._f));
}

TEST_CASE("SymmetricTensor algebra", "[symmetrictensor]") {
    SymmetricTensor t(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    REQUIRE(t.determinant() == -26._f);

    const Float detInv = 1._f / 26._f;
    SymmetricTensor inv(detInv * Vector(3._f, 1._f, -1._f), detInv * Vector(-9._f, -7._f, -5._f));
    REQUIRE(t.inverse() == approx(inv));
    REQUIRE_ASSERT(SymmetricTensor::null().inverse());

    SymmetricTensor t2(Vector(5._f, 3._f, -3._f), Vector(0._f));
    StaticArray<Float, 3> eigens = findEigenvalues(t2);
    // eigenvalues of diagonal matrix are diagonal elements
    REQUIRE(eigens[0] == approx(5._f, 1.e-5_f));
    REQUIRE(eigens[1] == approx(-3._f, 1.e-5_f));
    REQUIRE(eigens[2] == approx(3._f, 1.e-5_f));

    // double-dot product
    REQUIRE(ddot(t, t2) == 2._f);

    // outer product
    SymmetricTensor rhs(
        Vector(-5._f, -8.5_f, 16._f), Vector(-8.5_f, 12._f, -5._f), Vector(16._f, -5._f, -12._f));
    REQUIRE(outer(Vector(5._f, -3._f, -2._f), Vector(-1._f, -4._f, 6._f)) == rhs);
    REQUIRE(outer(Vector(-1._f, -4._f, 6._f), Vector(5._f, -3._f, -2._f)) == rhs);
}

TEST_CASE("SymmetricTensor eigendecomposition", "[symmetrictensor]") {
    SymmetricTensor t(Vector(3._f, 1._f, 3._f), Vector(2._f, 4._f, 2._f));
    StaticArray<Float, 3> eigens = findEigenvalues(t);
    std::sort(eigens.begin(), eigens.end());
    Tensor v;
    Vector e;
    tieToTuple(v, e) = eigenDecomposition(t);
    Vector e0 = e;
    std::sort(&e[0], &e[3]);
    REQUIRE(eigens[0] == approx(e[0]));
    REQUIRE(eigens[1] == approx(e[1]));
    REQUIRE(eigens[2] == approx(e[2]));

    REQUIRE(getNormalized(v.row(0)) == approx(getNormalized(Vector(1, 0, -1)), 1.e-6_f));
    REQUIRE(getNormalized(v.row(1)) == approx(getNormalized(Vector(1, -3.56155, 1)), 1.e-6_f));
    REQUIRE(getNormalized(v.row(2)) == approx(getNormalized(Vector(1, 0.561553, 1)), 1.e-6_f));

    REQUIRE(dot(v.row(0), v.row(1)) == approx(0._f));
    REQUIRE(dot(v.row(0), v.row(2)) == approx(0._f));
    REQUIRE(dot(v.row(1), v.row(2)) == approx(0._f));

    SymmetricTensor diag(e0, Vector(0._f));
    REQUIRE(transform(diag, v.transpose()) == approx(t));
}

TEST_CASE("SymmetricTensor norm", "[symmetrictensor]") {
    // norm, check that the implementation satisfies basic requirements
    REQUIRE(norm(SymmetricTensor::null()) == 0._f);
    SymmetricTensor t1(Vector(2._f, 1._f, -1._f), Vector(2._f, 3._f, -4._f));
    REQUIRE(norm(4._f * t1) == 4._f * norm(t1));
    SymmetricTensor t2(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    REQUIRE(norm(t1 + t2) <= norm(t1) + norm(t2));
}

TEST_CASE("Predefined SymmetricTensors", "[symmetrictensor]") {
    SymmetricTensor id = SymmetricTensor::identity();
    REQUIRE(id == SymmetricTensor(Vector(1, 0, 0), Vector(0, 1, 0), Vector(0, 0, 1)));
    REQUIRE(id * Vector(2._f, 5._f, 7._f) == Vector(2._f, 5._f, 7._f));

    SymmetricTensor zero = SymmetricTensor::null();
    REQUIRE(zero == SymmetricTensor(Vector(0, 0, 0), Vector(0, 0, 0), Vector(0, 0, 0)));
    REQUIRE(zero * Vector(2._f, 5._f, 7._f) == Vector(0._f, 0._f, 0._f));
}

TEST_CASE("SymmetricTensor trace", "[symmetrictensor]") {
    SymmetricTensor t(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    REQUIRE(t.trace() == 6._f);

    REQUIRE(SymmetricTensor::identity().trace() == 3);
    REQUIRE(SymmetricTensor::null().trace() == 0);

    REQUIRE((t - SymmetricTensor::identity() * t.trace() / 3._f).trace() == 0);
}

TEST_CASE("SymmetricTensor abs", "[symmetrictensor]") {
    SymmetricTensor t1(Vector(2._f, 1._f, -1._f), Vector(2._f, 0._f, -4._f));
    SymmetricTensor abst1(Vector(2._f, 1._f, 1._f), Vector(2._f, 0._f, 4._f));
    REQUIRE(abs(t1) == abst1);
}

TEST_CASE("SymmetricTensor almostEqual", "[symmetrictensor]") {
    auto testTensor = [](SymmetricTensor& t) {
        REQUIRE(almostEqual(t, t));
        REQUIRE_FALSE(almostEqual(t, -t));
        REQUIRE(almostEqual(t, (1._f + EPS) * t, 2._f * EPS));
        REQUIRE_FALSE(almostEqual(t, 1.1_f * t));
        REQUIRE(almostEqual(t, 1.1_f * t, 0.1_f));
        REQUIRE_FALSE(almostEqual(t, 1.1_f * t, 0.02_f));
    };

    SymmetricTensor t1(Vector(2._f, 1._f, -1._f), Vector(2._f, 0._f, -4._f));
    testTensor(t1);
    SymmetricTensor t2 = 1.e10_f * t1;
    testTensor(t2);
}

TEST_CASE("SymmetricTensor less", "[symmetrictensor]") {
    SymmetricTensor t1(Vector(2._f, 1._f, -1._f), Vector(2._f, 0._f, -4._f));
    SymmetricTensor t2(Vector(3._f, 1._f, 0._f), Vector(5._f, -1._f, -2._f));
    REQUIRE(less(t1, t2) == SymmetricTensor(Vector(1._f, 0._f, 1._f), Vector(1._f, 0._f, 1._f)));
}
