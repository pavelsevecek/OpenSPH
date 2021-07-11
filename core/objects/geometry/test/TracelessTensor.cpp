#include "objects/geometry/TracelessTensor.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("TracelessTensor construction", "[tracelesstensor]") {
    AlignedStorage<TracelessTensor> t1;
    REQUIRE_NOTHROW(t1.emplace());

    TracelessTensor t2(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    REQUIRE(t2.row(0) == Vector(1._f, 2._f, 3._f));
    REQUIRE(t2.row(1) == Vector(2._f, 2._f, 4._f));
    REQUIRE(t2.row(2) == Vector(3._f, 4._f, -3._f));

    REQUIRE(t2(0, 0) == 1._f);
    REQUIRE(t2(0, 1) == 2._f);
    REQUIRE(t2(0, 2) == 3._f);
    REQUIRE(t2(1, 0) == 2._f);
    REQUIRE(t2(1, 1) == 2._f);
    REQUIRE(t2(1, 2) == 4._f);
    REQUIRE(t2(2, 0) == 3._f);
    REQUIRE(t2(2, 1) == 4._f);
    REQUIRE(t2(2, 2) == -3._f);

    REQUIRE(t2.row(0) == Vector(1._f, 2._f, 3._f));
    REQUIRE(t2.row(1) == Vector(2._f, 2._f, 4._f));
    REQUIRE(t2.row(2) == Vector(3._f, 4._f, -3._f));

    SymmetricTensor t3(Vector(1._f, 3._f, -4._f), Vector(1.5_f, -5._f, 2._f));
    TracelessTensor t4(t3);
    REQUIRE(t4(0, 0) == 1._f);
    REQUIRE(t4(1, 1) == 3._f);
    REQUIRE(t4(2, 2) == -4._f);
    REQUIRE(t4(0, 1) == 1.5_f);
    REQUIRE(t4(1, 0) == 1.5_f);
    REQUIRE(t4(0, 2) == -5._f);
    REQUIRE(t4(2, 0) == -5._f);
    REQUIRE(t4(1, 2) == 2._f);
    REQUIRE(t4(2, 1) == 2._f);

    REQUIRE(t4.row(0) == Vector(1._f, 1.5_f, -5._f));
    REQUIRE(t4.row(1) == Vector(1.5_f, 3._f, 2._f));
    REQUIRE(t4.row(2) == Vector(-5._f, 2._f, -4._f));
}

TEST_CASE("TracelessTensor copy", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    TracelessTensor t2(t1);
    REQUIRE(t1 == t2);
    TracelessTensor t3;
    t3 = t1;
    REQUIRE(t1 == t3);

    SymmetricTensor t4 = SymmetricTensor(t1);
    REQUIRE(t1 == t4);

    TracelessTensor t5(t4);
    REQUIRE(t1 == t5);
    REQUIRE(t4 == t5);

    TracelessTensor t6;
    t6 = t4;
    REQUIRE(t4 == t6);
}

TEST_CASE("TracelessTensor operation", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    TracelessTensor t2(Vector(-1._f, 0._f, 1._f), Vector(0._f, -2._f, 1._f), Vector(1._f, 1._f, 3._f));

    REQUIRE(t1 + t2 ==
            TracelessTensor(Vector(0._f, 2._f, 4._f), Vector(2._f, 0._f, 5._f), Vector(4._f, 5._f, 0._f)));
    REQUIRE(t1 - t2 ==
            TracelessTensor(Vector(2._f, 2._f, 2._f), Vector(2._f, 4._f, 3._f), Vector(2._f, 3._f, -6._f)));

    /*REQUIRE(t1 * t2 ==
            TracelessTensor(Vector(-1._f, 0._f, 3._f), Vector(0._f, -4._f, 4._f), Vector(3._f, 4._f, -9._f)));
    REQUIRE(t2 / t1 == TracelessTensor(Vector(-1._f, 0._f, 1._f / 3._f),
                           Vector(0._f, -1._f, 0.25_f),
                           Vector(1._f / 3._f, 0.25f, -1._f)));*/
    REQUIRE(t1 * 2._f ==
            TracelessTensor(Vector(2._f, 4._f, 6._f), Vector(4._f, 4._f, 8._f), Vector(6._f, 8._f, -6._f)));
    REQUIRE(2._f * t1 ==
            TracelessTensor(Vector(2._f, 4._f, 6._f), Vector(4._f, 4._f, 8._f), Vector(6._f, 8._f, -6._f)));
    REQUIRE(t1 / 0.5_f ==
            TracelessTensor(Vector(2._f, 4._f, 6._f), Vector(4._f, 4._f, 8._f), Vector(6._f, 8._f, -6._f)));
    REQUIRE(-t1 == TracelessTensor(
                       Vector(-1._f, -2._f, -3._f), Vector(-2._f, -2._f, -4._f), Vector(-3._f, -4._f, 3._f)));
}


TEST_CASE("TracelessTensor apply", "[tracelesstensor]") {
    TracelessTensor t(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    Vector v(2._f, 1._f, -1._f);
    REQUIRE(t * v == Vector(1._f, 2._f, 13._f));
    v = Vector(0._f);
    REQUIRE(t * v == Vector(0._f));
}

TEST_CASE("TracelessTensor diagonal", "[tracelesstensor]") {
    TracelessTensor t1(5._f);
    REQUIRE(t1.diagonal() == Vector(5._f, 5._f, -10._f));
    REQUIRE(t1.offDiagonal() == Vector(5._f, 5._f, 5._f));
    TracelessTensor t2(Vector(1._f, 0._f, -1._f), Vector(0._f, 4._f, 6._f), Vector(-1._f, 6._f, -5._f));
    REQUIRE(t2.diagonal() == Vector(1._f, 4._f, -5._f));
    REQUIRE(t2.offDiagonal() == Vector(0._f, -1._f, 6._f));
}

TEST_CASE("TracelessTensor double-dot", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    TracelessTensor t2(Vector(-1._f, 0._f, 1._f), Vector(0._f, -2._f, 1._f), Vector(1._f, 1._f, 3._f));
    REQUIRE(ddot(t1, t2) == 0._f);

    SymmetricTensor t3(Vector(2._f, -1._f, 0._f), Vector(-1._f, 4._f, 3._f), Vector(0._f, 3._f, -2._f));
    REQUIRE(ddot(t1, t3) == 36._f);
    REQUIRE(ddot(t3, t1) == 36._f);
}

TEST_CASE("TracelessTensor algebra", "[tracelesstensor]") {
    TracelessTensor t1(5._f);
    REQUIRE(SymmetricTensor(t1).trace() == 0._f);

    /*REQUIRE(t1.maxElement() == 5);
    TracelessTensor t2(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    REQUIRE(t2.maxElement() == 4._f);*/
}

TEST_CASE("TracelessTensor norm", "[tracelesstensor]") {
    // norm, check that the implementation satisfies basic requirements
    REQUIRE(norm(TracelessTensor::null()) == 0._f);
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    REQUIRE(norm(4._f * t1) == 4._f * norm(t1));
    TracelessTensor t2(Vector(-1._f, 0._f, 1._f), Vector(0._f, -2._f, 1._f), Vector(1._f, 1._f, 3._f));
    REQUIRE(norm(t1 + t2) <= norm(t1) + norm(t2));
}

TEST_CASE("TracelessTensor minElement", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    REQUIRE(minElement(t1) == -3._f);

    TracelessTensor t2(Vector(5._f, 4._f, 2._f), Vector(4._f, -7._f, 9._f), Vector(2._f, 9._f, 2._f));
    REQUIRE(minElement(t2) == -7._f);
}

TEST_CASE("TracelessTensor abs", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, -2._f, 1._f), Vector(-2._f, -2._f, 4._f), Vector(1._f, 4._f, 1._f));
    SymmetricTensor abst1(Vector(1._f, 2._f, 1._f), Vector(2._f, 2._f, 4._f), Vector(1._f, 4._f, 1._f));
    REQUIRE(abs(t1) == abst1);
}

TEST_CASE("TracelessTensor almostEqual", "[tracelesstensor]") {
    auto testTensor = [](TracelessTensor& t) {
        REQUIRE(almostEqual(t, t));
        REQUIRE_FALSE(almostEqual(t, -t));
        REQUIRE(almostEqual(t, (1._f + EPS) * t, 2._f * EPS));
        REQUIRE_FALSE(almostEqual(t, 1.1_f * t));
        REQUIRE(almostEqual(t, 1.1_f * t, 0.1_f));
        REQUIRE_FALSE(almostEqual(t, 1.1_f * t, 0.02_f));
    };

    TracelessTensor t1(Vector(1._f, -2._f, 1._f), Vector(-2._f, -2._f, 4._f), Vector(1._f, 4._f, 1._f));
    testTensor(t1);
    TracelessTensor t2 = 1.e10_f * t1;
    testTensor(t2);
}

TEST_CASE("TracelessTensor equality", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, -2._f, 1._f), Vector(-2._f, -2._f, 4._f), Vector(1._f, 4._f, 1._f));
    SymmetricTensor t2(Vector(1._f, -2._f, 1._f), Vector(-2._f, 1._f, 4._f));
    SymmetricTensor t3(Vector(1._f, -2._f, 1._f), Vector(-2._f, 1._f, 5._f));
    SymmetricTensor t4(Vector(1._f, -2.5_f, 1._f), Vector(-2._f, 1._f, 4._f));

    REQUIRE(t1 == t1);
    REQUIRE_FALSE(t1 != t1);
    REQUIRE(t1 == t2);
    REQUIRE_FALSE(t1 != t2);
    REQUIRE(t2 == t1);
    REQUIRE_FALSE(t2 != t1);
    REQUIRE(t1 != t3);
    REQUIRE(t1 != t4);
    REQUIRE(t3 != t1);
    REQUIRE(t4 != t1);
}

TEST_CASE("TracelessTensor clamp", "[tracelesstensor]") {
    TracelessTensor t1(Vector(0._f, -2._f, 1._f), Vector(-2._f, 0._f, 4._f), Vector(1._f, 4._f, 0._f));
    // off diagonal components are clamped normally
    const Interval r(-1._f, 1._f);
    TracelessTensor expected1(Vector(0._f, -1._f, 1._f), Vector(-1._f, 0._f, 1._f), Vector(1._f, 1._f, 0._f));
    REQUIRE(clamp(t1, r) == expected1);

    // diagonal components are clamped and the trace is subtracted from result
    TracelessTensor t2(Vector(1._f, -2._f, 3._f), Vector(-2._f, -6._f, 4._f), Vector(3._f, 4._f, 5._f));
    SymmetricTensor expected2(Vector(1._f, -1._f, 1._f), Vector(-1._f, 1._f, 1._f));
    REQUIRE(clamp(t2, r) ==
            approx(TracelessTensor(expected2 - SymmetricTensor::identity() * expected2.trace() / 3._f)));
}

TEST_CASE("TracelessTensor less", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    TracelessTensor t2(Vector(5._f, 4._f, 2._f), Vector(4._f, -7._f, 9._f), Vector(2._f, 9._f, 2._f));
}

TEST_CASE("TracelessTensor convert", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    TracelessTensor t2 = convert<TracelessTensor>(convert<AffineMatrix>(t1));
    REQUIRE(t1 == t2);
}
