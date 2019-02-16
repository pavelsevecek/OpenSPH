#include "physics/Eos.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "system/Settings.h"
#include "tests/Approx.h"

using namespace Sph;

TEST_CASE("Ideal gas", "[eos]") {}

TEST_CASE("Tillotson values", "[eos]") {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 2.7_f);
    settings.set(BodySettingsId::TILLOTSON_SUBLIMATION, 1.e5_f);
    TillotsonEos eos(settings);

    Float p, cs;

    tie(p, cs) = eos.evaluate(2.7_f, 1.e5_f);
    REQUIRE(p == approx(337500._f, 1.e-6_f));
    REQUIRE(cs == approx(99444.4453_f, 1.e-6_f));

    tie(p, cs) = eos.evaluate(2.0_f, 1.e5_f);
    REQUIRE(p == approx(-5.12736563e9_f, 1.e-6_f));
    REQUIRE(cs == approx(54744.2812_f, 1.e-6_f));

    tie(p, cs) = eos.evaluate(2.0_f, 1.e10_f);
    REQUIRE(p == approx(9.34812365e9_f, 1.e-6_f));
    REQUIRE(cs == approx(67291.1719_f, 1.e-6_f));

    tie(p, cs) = eos.evaluate(2.8_f, 1.e10_f);
    REQUIRE(p == approx(1.50259651e10, 1.e-6_f));
    REQUIRE(cs == approx(135296.312_f, 1.e-6_f));

    tie(p, cs) = eos.evaluate(2.6_f, 1.e7_f);
    REQUIRE(p == approx(-883133952._f, 1.e-6_f));
    REQUIRE(cs == approx(88856.2188_f, 1.e-6_f));

    tie(p, cs) = eos.evaluate(2.7_f, 1.e7_f);
    REQUIRE(p == approx(13900990._f, 1.e-6_f));
    REQUIRE(cs == approx(99483.1953_f, 1.e-6_f));

    tie(p, cs) = eos.evaluate(2.8_f, 1.e7_f);
    REQUIRE(p == approx(1.03996064e9_f, 1.e-6_f));
    REQUIRE(cs == approx(103983.867_f, 1.e-6_f));
}

TEST_CASE("Tillotson continuous", "[eos]") {
    // test that EoS is a continuous function of p and u
    /// \todo Tillotson eos(BodySettings::getDefaults());
}

TEST_CASE("Tillotson inverted energy", "[eos]") {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 2.7_f);
    settings.set(BodySettingsId::TILLOTSON_SUBLIMATION, 1.e8_f);
    TillotsonEos eos(settings);

    auto test = [&](const Float u0, const Float rho0, const Float eps = 2.e-6_f) {
        Float p, cs;
        tie(p, cs) = eos.evaluate(rho0, u0);
        REQUIRE(eos.getInternalEnergy(rho0, p) == approx(u0, eps));
    };

    test(0._f, 2.7_f);
    test(100._f, 2.7_f);

    test(1.e4_f, 2.4_f);
    test(1.e7_f, 2.4_f);
    test(1.e8_f, 2.4_f);

    test(1.e7_f, 2.7_f);
    test(1.e8_f, 2.7_f);
    test(1.e4_f, 2.7_f);

    test(1.e7_f, 3.0_f);
    test(1.e8_f, 3.0_f);
    test(1.e4_f, 3.0_f);
}

TEST_CASE("Tillotson inverted density", "[eos]") {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 2.7_f);
    settings.set(BodySettingsId::TILLOTSON_SUBLIMATION, 1.e8_f);
    TillotsonEos eos(settings);
    auto test = [&](const Float u0, const Float rho0, const Float eps = 1.e-6_f) {
        Float p, cs;
        tie(p, cs) = eos.evaluate(rho0, u0);
        REQUIRE(eos.getDensity(p, u0) == approx(rho0, eps));
    };

    /// \todo the function only works for densities close to rho0
    test(0._f, 2.7_f);
    test(100._f, 2.7_f);
    test(1.e4_f, 2.7_f);
    test(1.e7_f, 2.7_f);
    test(1.e8_f, 2.7_f);

    test(1.e4_f, 2.71_f);
    test(1.e7_f, 2.71_f);
    test(1.e8_f, 2.71_f);

    test(1.e4_f, 2.69_f);
    test(1.e7_f, 2.69_f);
    test(1.e8_f, 2.69_f);
}
