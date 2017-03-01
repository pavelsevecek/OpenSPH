#include "physics/Units.h"
#include "catch.hpp"
#include "utils/Approx.h"

using namespace Sph;

// some non-trivial system to check conversions
const UnitSystem testSystem(0.5_f, 3.5_f, 0.2_f, 0.7_f, 1.2_f, 10._f, 100._f);

TEST_CASE("Dimensionless construction", "[units]") {
    REQUIRE_NOTHROW(Unit defaultConstructed);
    Unit u(5._f, testSystem, Dimensions::dimensionless());

    REQUIRE(u.value(testSystem) == 5._f);
    REQUIRE(u.value(UnitSystem::SI()) == 5._f);
    REQUIRE(u.value(UnitSystem::CGS()) == 5._f);
}

TEST_CASE("Unit construction", "[units]") {
    Unit u1(2._f, testSystem, Dimensions::length()); // 2 * testSystem[LENGTH];
    REQUIRE(u1.value(testSystem) == 2._f);
    REQUIRE(u1.value(UnitSystem::SI()) == 2._f * testSystem[Dimensions::LENGTH]);

    Unit u2(3._f, testSystem, Dimensions::density());
    REQUIRE(u2.value(testSystem) == 3._f);
    REQUIRE(u2.value(UnitSystem::SI()) ==
            3._f * testSystem[Dimensions::MASS] / pow<3>(testSystem[Dimensions::LENGTH]));
}
/*
TEST_CASE("BaseUnit conversion", "[units]") {
    using T = Evt::T;
    // implicit conversion for dimensionless
    Dimensionless<> dimless = T(5);
    float f = dimless;
    REQUIRE(f == T(5));

    // explicit conversion for units with dimensions
    Length<> length = 2._m;
    f = Evt::T(length);
    REQUIRE(f == T(2));
}

TEST_CASE("BaseUnit operators", "[units]") {
    using T = Evt::T;
    // copy operator for dimensionless values
    Dimensionless<> dimless = T(1);
    dimless = T(3);
    REQUIRE(dimless.value() == T(3));

    // generic copy operator
    Mass<> mass;
    mass = 2._kg;
    REQUIRE(mass.value() == T(2));

    // add/subtract
    mass += 3._kg;
    REQUIRE(mass.value() == T(5));
    mass -= 2._kg;
    REQUIRE(mass.value() == T(3));

    // multiply/divide
    mass *= T(2);
    REQUIRE(mass.value() == T(6));
    mass /= T(4);
    REQUIRE(mass.value() == T(1.5));

    // unary minus
    Mass<> negativeMass = -mass;
    REQUIRE(negativeMass.value() == T(-1.5));
}

TEST_CASE("BaseUnit comparators", "[units]") {
    Length<> l1 = 3._m;
    Length<> l2 = 300._cm;
    Length<> l3 = 2._m;

    REQUIRE(l1 == l2);
    REQUIRE(l1 != l3);
    REQUIRE(l2 != l3);
    REQUIRE(l1 > l3);
    REQUIRE(l2 > l3);
    REQUIRE(l3 < l1);
    REQUIRE(l3 < l2);
    REQUIRE(l1 >= l3);
    REQUIRE(l2 >= l3);
    REQUIRE(l3 <= l1);
    REQUIRE(l3 <= l2);
    REQUIRE(l1 <= l2);
    REQUIRE(l2 >= l1);
}

TEST_CASE("Unit value", "[units]") {
    Unit u1 = 2._m;
    Float value = u1.value(UnitSystem::CGS());
    REQUIRE(f == 200); // cm
}

TEST_CASE("BaseUnit operators 2", "[units]") {
    // add/subtract
    Time<> t1 = 2._h;
    Time<> t2 = 3._h;
    REQUIRE(t1 + t2 == 5._h);
    REQUIRE(t1 - t2 == -1._h);

    using T = Evt::T;
    // multiply/divide by value
    REQUIRE(t1 * T(2) == 4._h);
    REQUIRE(T(3) * t2 == 9._h);
    REQUIRE(t1 / T(2) == 1._h);
    Inverted<Time<>> lhs = T(9) / t2;
    Inverted<Time<>> rhs(T(3), Units::get<T>(1._h));
    REQUIRE(lhs == rhs);

    // multiply/divide by other unit
    Length<> l1 = 5._km;
    Product<Length<>, Time<>> p1(T(10), Units::get<T>(1._km, 1._h));
    REQUIRE(t1 * l1 == p1);
    Mass<> m1 = 2._kg;
    Quotient<Time<>, Mass<>> q1(T(1.5), Units::get<T>(1._h));
    REQUIRE(q1 == t2 / m1);
}

TEST_CASE("Units::setDefault", "[units]") {
    // set some units as a default
    using T = Evt::T;
    Units::setDefault<>(1._g, 20._m, 30._min, 20._deg);
    auto a = T(20) * T(20) * T(30) * 1._g * 1._m * 1._min * 1._deg;
    // auto a = 20._m; // 1._g * 20._m * 30._min * 20._deg;
    REQUIRE(Math::almostEqual(a.value(), T(1)));
    Units::actual = Units::SI<>;
}
*/
