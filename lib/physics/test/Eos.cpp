#include "physics/Eos.h"
#include "catch.hpp"
#include "objects/containers/Array.h"
#include "utils/Approx.h"
#include "system/Settings.h"

using namespace Sph;

TEST_CASE("Ideal gas", "[eos]") {}

TEST_CASE("Tillotson", "[eos]") {
    BodySettings settings;
    settings.set(BodySettingsIds::DENSITY, 2.7_f);
    settings.set(BodySettingsIds::TILLOTSON_SUBLIMATION, 1.e5_f);
    TillotsonEos eos(settings);

    Float p, cs;

    tieToTuple(p, cs) = eos.evaluate(2.7_f, 1.e5_f);
    REQUIRE(p == approx(337500._f, 1.e-6_f));
    REQUIRE(cs == approx(99444.4453_f, 1.e-6_f));

    tieToTuple(p, cs) = eos.evaluate(2.0_f, 1.e5_f);
    REQUIRE(p == approx(-5.12736563e9_f, 1.e-6_f));
    REQUIRE(cs == approx(54744.2812_f, 1.e-6_f));

    tieToTuple(p, cs) = eos.evaluate(2.0_f, 1.e10_f);
    REQUIRE(p == approx(9.34812365e9_f, 1.e-6_f));
    REQUIRE(cs == approx(67291.1719_f, 1.e-6_f));

    tieToTuple(p, cs) = eos.evaluate(2.8_f, 1.e10_f);
    REQUIRE(p == approx(1.50259651e10, 1.e-6_f));
    REQUIRE(cs == approx(135296.312_f, 1.e-6_f));

    tieToTuple(p, cs) = eos.evaluate(2.6_f, 1.e7_f);
    REQUIRE(p == approx(-883133952._f, 1.e-6_f));
    REQUIRE(cs == approx(88856.2188_f, 1.e-6_f));

    tieToTuple(p, cs) = eos.evaluate(2.7_f, 1.e7_f);
    REQUIRE(p == approx(13900990._f, 1.e-6_f));
    REQUIRE(cs == approx(99483.1953_f, 1.e-6_f));

    tieToTuple(p, cs) = eos.evaluate(2.8_f, 1.e7_f);
    REQUIRE(p == approx(1.03996064e9_f, 1.e-6_f));
    REQUIRE(cs == approx(103983.867_f, 1.e-6_f));
}
