#include "physics/Units.h"
#include "catch.hpp"
#include "utils/Approx.h"

using namespace Sph;

// some non-trivial system to check conversions
const UnitSystem testSystem(0.5_f, 3.5_f, 0.2_f, 0.7_f, 1.2_f, 10._f, 100._f);

class UnitSystemGuard {
private:
    UnitSystem original;

public:
    UnitSystemGuard(const UnitSystem& newSystem)
        : original(UnitSystem::code()) {
        UnitSystem::code() = newSystem;
    }

    ~UnitSystemGuard() {
        UnitSystem::code() = original;
    }
};


TEST_CASE("Dimensions equality", "[units]") {
    Dimensions d1 = Dimensions::length();
    Dimensions d2 = Dimensions::area();
    Dimensions d3 = Dimensions::time();
    Dimensions d4 = Dimensions::dimensionless();
    Dimensions d5 = Dimensions(0, 1, 0, 0, 0, 0, 0);
    REQUIRE(d1 == d1);
    REQUIRE(d1 == d5);
    REQUIRE_FALSE(d1 == d2);
    REQUIRE_FALSE(d1 == d3);
    REQUIRE_FALSE(d1 == d4);
    REQUIRE_FALSE(d2 == d3);
}

TEST_CASE("UnitSystem conversion", "[units]") {
    // conversion factor SI->CGS for length
    REQUIRE(UnitSystem::CGS().conversion(Dimensions::length()) == 1.e2_f); // x[cm] = 100 * x[m]
    REQUIRE(UnitSystem::CGS().conversion(Dimensions::area()) == approx(1.e4_f));
    REQUIRE(UnitSystem::CGS().conversion(Dimensions::volume()) == approx(1.e6_f));
    REQUIRE(UnitSystem::CGS().conversion(Dimensions::density()) == approx(1.e-3_f));
}

TEST_CASE("Dimensionless construction", "[units]") {
    REQUIRE_NOTHROW(Unit defaultConstructed);
    Unit u(5._f, testSystem, Dimensions::dimensionless());

    REQUIRE(u.value(testSystem) == 5._f);
    REQUIRE(u.value(UnitSystem::SI()) == 5._f);
    REQUIRE(u.value(UnitSystem::CGS()) == 5._f);
    REQUIRE(u.dimensions() == Dimensions::dimensionless());
}

static void unitConstructionTests(const UnitSystem& internalSystem) {
    UnitSystemGuard guard(internalSystem);
    Unit u1(2._f, testSystem, Dimensions::length()); // 2 * testSystem[LENGTH];
    REQUIRE(u1.value(testSystem) == 2._f);
    REQUIRE(u1.value(UnitSystem::SI()) == 2._f * testSystem[Dimensions::LENGTH]);
    REQUIRE(u1.dimensions() == Dimensions::length());

    Unit u2(3._f, testSystem, Dimensions::density());
    REQUIRE(u2.value(testSystem) == 3._f);
    REQUIRE(u2.value(UnitSystem::SI()) ==
            3._f * testSystem[Dimensions::MASS] / pow<3>(testSystem[Dimensions::LENGTH]));
    REQUIRE(u2.dimensions() == Dimensions::density());
}

TEST_CASE("Unit construction", "[units]") {
    // internal system should not affect the results in any way
    unitConstructionTests(UnitSystem::SI());
    unitConstructionTests(UnitSystem::CGS());
    unitConstructionTests(UnitSystem(5._f, 2._f, 0.001_f));
}

static void unitAssignmentTests(const UnitSystem& internalSystem) {
    UnitSystemGuard guard(internalSystem);
    Unit u1(1._f, UnitSystem::SI(), Dimensions::dimensionless());
    u1 = Unit(3._f, testSystem, Dimensions::length());
    REQUIRE(u1.value(testSystem) == 3._f);
    REQUIRE(u1.dimensions() == Dimensions::length());

    Unit u2;
    u2 = u1;
    REQUIRE(u2.value(testSystem) == 3._f);
    REQUIRE(u2.dimensions() == Dimensions::length());
}

TEST_CASE("Unit assignment", "[units]") {
    unitAssignmentTests(UnitSystem::SI());
    unitAssignmentTests(UnitSystem::CGS());
    unitAssignmentTests(UnitSystem(2._f, 3._f, 4._f));
}

TEST_CASE("Unit literals", "[units]") {
    Unit u1 = 5._m;
    REQUIRE(u1.dimensions() == Dimensions::length());
    REQUIRE(u1.value(UnitSystem::SI()) == 5._f);
    REQUIRE(u1.value(UnitSystem::CGS()) == 500._f);

    Unit u2;
    u2 = 10._s;
    REQUIRE(u2.dimensions() == Dimensions::time());
    REQUIRE(u2.value(UnitSystem::SI()) == 10._f);
    REQUIRE(u2.value(UnitSystem::CGS()) == 10._f);
}

TEST_CASE("Unit sum/difference", "[units]") {
    Unit u1 = 5._m + 300._cm;
    REQUIRE(u1.dimensions() == Dimensions::length());
    REQUIRE(u1.value(UnitSystem::SI()) == 8._f);
    /// \todo tests assets by a global switch that either asserts of throws exception
    Unit u2 = 7._min - 2._min;
    REQUIRE(u2.dimensions() == Dimensions::time());
    REQUIRE(u2.value(UnitSystem::SI()) == 300._f);

    REQUIRE_THROWS(5._m + 3._kg);
    REQUIRE_THROWS(5._s - 2._km);
}

TEST_CASE("Unit product/quotient", "[units]") {
    Unit u1 = 5._m * 2._f;
    REQUIRE(u1.dimensions() == Dimensions::length());
    REQUIRE(u1.value(UnitSystem::SI()) == 10._f);

    Unit u2 = 3._f * 5._m;
    REQUIRE(u2.dimensions() == Dimensions::length());
    REQUIRE(u2.value(UnitSystem::SI()) == 15._f);

    Unit u3 = 12._s / 4._f;
    REQUIRE(u3.dimensions() == Dimensions::time());
    REQUIRE(u3.value(UnitSystem::SI()) == 3._f);

    Unit u4 = 5._m * 200._cm;
    REQUIRE(u4.dimensions() == Dimensions::area());
    REQUIRE(u4.value(UnitSystem::SI()) == 10._f);

    Unit u5 = 4._kg / 2._m;
    REQUIRE(u5.dimensions() == Dimensions(1, -1));
    REQUIRE(u5.value(UnitSystem::SI()) == 2._f);
}


/*TEST_CASE("BaseUnit comparators", "[units]") {
 * /// \todo check that asserts work
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
}*/
