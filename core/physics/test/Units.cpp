#include "physics/Units.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Unit systems", "[units]") {
    Unit u1(5._f, BasicDimension::MASS, UnitSystem::CGS());
    REQUIRE(u1 == approx(5._g));
    REQUIRE(u1.value(UnitSystem::CGS()) == approx(5._f));
    REQUIRE(u1.value(UnitSystem::SI()) == approx(5.e-3_f));
}

TEST_CASE("Unit sum and diff", "[units]") {
    Unit u1 = 5._m;
    Unit u2 = 7._m;
    REQUIRE(u1 + u2 == 12._m);
    REQUIRE(u1 - u2 == -2._m);
    u1 += u2;
    REQUIRE(u1 == 12._m);
    u2 -= u1;
    REQUIRE(u2 == -5._m);
}

TEST_CASE("Unit product and div", "[units]") {
    Unit u1 = 6._m;
    Unit u2 = 3._s;
    Unit prod = u1 * u2;
    REQUIRE(prod.value(UnitSystem::SI()) == 18._f);
    REQUIRE(prod.dimension() == UnitDimensions::length() + UnitDimensions::time());

    Unit diff = u1 / u2;
    REQUIRE(diff.value(UnitSystem::SI()) == 2._f);
    REQUIRE(diff.dimension() == UnitDimensions::velocity());
    REQUIRE(diff == 2._mps);

    u1 *= 5._f;
    REQUIRE(u1 == 30._m);
    u2 *= 2._kg;
    REQUIRE(u2.value(UnitSystem::SI()) == 6);
    REQUIRE(u2.dimension() == UnitDimensions::mass() + UnitDimensions::time());

    u2 /= 6._g;
    REQUIRE(u2 == approx(1000._s));
}

TEST_CASE("Unit invalid operations", "[units]") {
    Unit u1 = 6._m;
    Unit u2 = 3._s;
    REQUIRE_SPH_ASSERT(u1 + u2);
    REQUIRE_SPH_ASSERT(u1 - u2);
}

TEST_CASE("Unit parseUnit", "[units]") {
    SKIP_TEST;
    Expected<Unit> u1 = parseUnit("m");
    REQUIRE(u1.value() == 1._m);
    Expected<Unit> u2 = parseUnit("km h^-1");
    REQUIRE(u2.value() == approx(0.27777777_mps, 1.e-6_f));
    Expected<Unit> u3 = parseUnit("");
    REQUIRE(u3.value() == Unit::dimensionless(1._f));
    Expected<Unit> u4 = parseUnit("kg m^2 s^-2");
    REQUIRE(u4.value().value(UnitSystem::SI()) == 1._f);
    REQUIRE(u4.value().dimension() == UnitDimensions::energy());

    REQUIRE_FALSE(parseUnit("kgm"));
    REQUIRE_FALSE(parseUnit("m^2s"));
    REQUIRE_FALSE(parseUnit("m^2^3"));
    REQUIRE_FALSE(parseUnit("kg^ "));
}

TEST_CASE("Unit print", "[units") {
    SKIP_TEST;

    auto print = [](Unit u) {
        std::stringstream ss;
        ss << u;
        return ss.str();
    };

    REQUIRE(print(1200._m) == "1.2km");
    REQUIRE(print(400._m) == "400m");
    REQUIRE(print(0.8_m) == "80cm");
    REQUIRE(print(0.004_m) == "4mm");
    REQUIRE(print(0.0001_m) == "0.1mm");

    REQUIRE(print(Unit::dimensionless(1._f) / Unit::second(1.e4_f)) == "1.e-4s^-1");
}
