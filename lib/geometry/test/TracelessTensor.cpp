#include "geometry/TracelessTensor.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("TracelessTensor construction", "[tracelesstensor]") {
    REQUIRE_NOTHROW(TracelessTensor t1);

    TracelessTensor t2(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    REQUIRE(t2[0] == Vector(1._f, 2._f, 3._f));
    REQUIRE(t2[1] == Vector(2._f, 2._f, 4._f));
    REQUIRE(t2[2] == Vector(3._f, 4._f, -3._f));

    REQUIRE(t2(0, 0) == 1._f);
    REQUIRE(t2(0, 1) == 2._f);
    REQUIRE(t2(0, 2) == 3._f);
    REQUIRE(t2(1, 0) == 2._f);
    REQUIRE(t2(1, 1) == 2._f);
    REQUIRE(t2(1, 2) == 4._f);
    REQUIRE(t2(2, 0) == 3._f);
    REQUIRE(t2(2, 1) == 4._f);
    REQUIRE(t2(2, 2) == -3._f);
}

TEST_CASE("TracelessTensor copy", "[tracelesstensor]") {
    TracelessTensor t1(Vector(1._f, 2._f, 3._f), Vector(2._f, 2._f, 4._f), Vector(3._f, 4._f, -3._f));
    TracelessTensor t2(t1);
    REQUIRE(t1 == t2);
    TracelessTensor t3;
    t3 = t1;
    REQUIRE(t1 == t3);

    Tensor t4 = Tensor(t1);
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

    REQUIRE(t1 * t2 ==
            TracelessTensor(Vector(-1._f, 0._f, 3._f), Vector(0._f, -4._f, 4._f), Vector(3._f, 4._f, -9._f)));
    REQUIRE(t2 / t1 == TracelessTensor(Vector(-1._f, 0._f, 1._f / 3._f),
                           Vector(0._f, -1._f, 0.25_f),
                           Vector(1._f / 3._f, 0.25f, -1._f)));
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

    Tensor t3(Vector(2._f, -1._f, 0._f), Vector(-1._f, 4._f, 3._f), Vector(0._f, 3._f, -2._f));
    REQUIRE(ddot(t1, t3) == 36._f);
    REQUIRE(ddot(t3, t1) == 36._f);
}

TEST_CASE("TracelessTensor algebra", "[tracelesstensor]") {
    TracelessTensor t1(5._f);
    REQUIRE(Tensor(t1).trace() == 0._f);

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
    Tensor abst1(Vector(1._f, 2._f, 1._f), Vector(2._f, 2._f, 4._f), Vector(1._f, 4._f, 1._f));
    REQUIRE(abs(t1) == abst1);
}
