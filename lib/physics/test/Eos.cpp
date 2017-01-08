#include "physics/Eos.h"
#include "catch.hpp"
#include "objects/containers/Array.h"

using namespace Sph;

TEST_CASE("Ideal gas", "[eos]") {}

TEST_CASE("Tillotson", "[eos]") {
    Settings<BodySettingsIds> settings(BODY_SETTINGS);
    settings.set(BodySettingsIds::DENSITY, 2.7_f);
    settings.set(BodySettingsIds::TILLOTSON_SUBLIMATION, 1.e5_f);
    TillotsonEos eos(settings);

    Float p, cs;

    tieToTuple(p, cs) = eos.getPressure(2.7_f, 1.e5_f);
    REQUIRE(almostEqual(p, 337500._f));
    REQUIRE(almostEqual(cs, Sph::sqrt(9.88919808e9_f)));

    tieToTuple(p, cs) = eos.getPressure(2.0_f, 1.e5_f);
    REQUIRE(almostEqual(p, -5.12736563e9_f));
    REQUIRE(almostEqual(cs, Sph::sqrt(2.99693619e9_f)));

    tieToTuple(p, cs) = eos.getPressure(2.0_f, 1.e10_f);
    REQUIRE(almostEqual(p, 9.34812365e9_f));
    REQUIRE(almostEqual(cs, Sph::sqrt(4.52810138e9_f)));

    tieToTuple(p, cs) = eos.getPressure(2.8_f, 1.e10_f);
    REQUIRE(almostEqual(p, 1.50259651e10));
    REQUIRE(almostEqual(cs, Sph::sqrt(1.83050916e10)));

    tieToTuple(p, cs) = eos.getPressure(2.6_f, 1.e7_f);
    REQUIRE(almostEqual(p, -883133952._f));
    REQUIRE(almostEqual(cs, Sph::sqrt(7.89542707e9_f)));

    tieToTuple(p, cs) = eos.getPressure(2.7_f, 1.e7_f);
    REQUIRE(almostEqual(p, 13900990._f));
    REQUIRE(almostEqual(cs, Sph::sqrt(9.89690573e9_f)));

    tieToTuple(p, cs) = eos.getPressure(2.8_f, 1.e7_f);
    REQUIRE(almostEqual(p, 1.03996064e9_f));
    REQUIRE(almostEqual(cs, Sph::sqrt(1.08126444e10_f)));
}
