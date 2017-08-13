#include "geometry/AntisymmetricTensor.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;


TEST_CASE("AntisymmetricTensor construction", "[antisymmetrictensor]") {
    REQUIRE_NOTHROW(AntisymmetricTensor t1);

    AntisymmetricTensor t2(Vector(1._f, 2._f, 3._f));
    REQUIRE(t2[0] == Vector(0._f, 1._f, 2._f));
    REQUIRE(t2[1] == Vector(-1._f, 0._f, 3._f));
    REQUIRE(t2[2] == Vector(-2._f, -3._f, 0._f));

    REQUIRE(t2(0, 0) == 0._f);
    REQUIRE(t2(0, 1) == 1._f);
    REQUIRE(t2(0, 2) == 2._f);
    REQUIRE(t2(1, 0) == -1._f);
    REQUIRE(t2(1, 1) == 0._f);
    REQUIRE(t2(1, 2) == 3._f);
    REQUIRE(t2(2, 0) == -2._f);
    REQUIRE(t2(2, 1) == -3._f);
    REQUIRE(t2(2, 2) == 0._f);

    AntisymmetricTensor t3(PSEUDOVECTOR, Vector(3._f, -2._f, 1._f));
    REQUIRE(t2 == t3);
}

TEST_CASE("AntisymmetricTensor operations", "[antisymmetrictensor]") {
    AntisymmetricTensor t1(Vector(2._f, 1._f, -1._f));
    AntisymmetricTensor t2(Vector(1._f, 2._f, 3._f));
    REQUIRE(t1 + t2 == AntisymmetricTensor(Vector(3._f, 3._f, 2._f)));
    REQUIRE(t1 - t2 == AntisymmetricTensor(Vector(1._f, -1._f, -4._f)));
    AntisymmetricTensor t3 = AntisymmetricTensor::null();
    t3 += t1;
    REQUIRE(t3 == t1);
    t3 -= t2;
    REQUIRE(t3 == t1 - t2);
    REQUIRE(3._f * t1 == AntisymmetricTensor(Vector(6._f, 3._f, -3._f)));
    REQUIRE(3._f * t1 == t1 * 3._f);

    REQUIRE(t1 / 2._f == AntisymmetricTensor(Vector(1._f, 0.5_f, -0.5_f)));

    REQUIRE(-t1 == AntisymmetricTensor(Vector(-2._f, -1._f, 1._f)));
}

TEST_CASE("AntisymmetricTensor pseudovector", "[antisymmetrictensor]") {
    AntisymmetricTensor t1(PSEUDOVECTOR, Vector(2._f, 4._f, -1._f));
    REQUIRE(t1.pseudovector() == Vector(2._f, 4._f, -1._f));

    const Vector u1(2._f, -1._f, 5._f);
    const Vector u2(7._f, -3._f, 4._f);
    REQUIRE(2._f * antisymmetricOuter(u1, u2).pseudovector() == cross(u1, u2));
}

/*TEST_CASE("Tensor apply", "[tensor]") {
    Tensor t(Vector(1._f, 2._f, 3._f), Vector(-1._f, -2._f, -3._f));
    Vector v(2._f, 1._f, -1._f);

    REQUIRE(t * v == Vector(3._f, 3._f, -10._f));
}*/

TEST_CASE("Antisymmetrictensor norm", "[antisymmetrictensor]") {
    // norm, check that the implementation satisfies basic requirements
    REQUIRE(norm(AntisymmetricTensor::null()) == 0._f);
    AntisymmetricTensor t1(Vector(2._f, 1._f, -1._f));
    REQUIRE(norm(4._f * t1) == 4._f * norm(t1));
    AntisymmetricTensor t2(Vector(1._f, 2._f, 3._f));
    REQUIRE(norm(t1 + t2) <= norm(t1) + norm(t2));
}

TEST_CASE("Antisymmetric null", "[antisymmetrictensor]") {
    AntisymmetricTensor zero = AntisymmetricTensor::null();
    REQUIRE(zero == AntisymmetricTensor(Vector(0, 0, 0)));
}

TEST_CASE("Antisymmetric abs", "[antisymmetrictensor]") {
    AntisymmetricTensor t1(Vector(2._f, 1._f, -1._f));
    SymmetricTensor abst1(Vector(0._f), Vector(2._f, 1._f, 1._f));
    REQUIRE(abs(t1) == abst1);
}
/*
TEST_CASE("Tensor almostEqual", "[tensor]") {
    auto testTensor = [](Tensor& t) {
        REQUIRE(almostEqual(t, t));
        REQUIRE_FALSE(almostEqual(t, -t));
        REQUIRE(almostEqual(t, (1._f + EPS) * t, 2._f * EPS));
        REQUIRE_FALSE(almostEqual(t, 1.1_f * t));
        REQUIRE(almostEqual(t, 1.1_f * t, 0.1_f));
        REQUIRE_FALSE(almostEqual(t, 1.1_f * t, 0.02_f));
    };

    Tensor t1(Vector(2._f, 1._f, -1._f), Vector(2._f, 0._f, -4._f));
    testTensor(t1);
    Tensor t2 = 1.e10_f * t1;
    testTensor(t2);
}

TEST_CASE("Tensor less", "[tensor]") {
    Tensor t1(Vector(2._f, 1._f, -1._f), Vector(2._f, 0._f, -4._f));
    Tensor t2(Vector(3._f, 1._f, 0._f), Vector(5._f, -1._f, -2._f));
    REQUIRE(less(t1, t2) == Tensor(Vector(1._f, 0._f, 1._f), Vector(1._f, 0._f, 1._f)));
}*/
