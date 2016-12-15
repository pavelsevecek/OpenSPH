#include "physics/Eos.h"
#include "catch.hpp"
#include "objects/containers/Array.h"

using namespace Sph;

TEST_CASE("Ideal gas", "[eos]") {}

TEST_CASE("Tillotson", "[eos]") {
    Settings<BodySettingsIds> settings(BODY_SETTINGS);
    settings.set(BodySettingsIds::DENSITY, 2.7_f);
    settings.set(BodySettingsIds::ENERGY, 1.e5_f);
    TillotsonEos eos(settings);

    Float p, cs;

    tieToTuple(p, cs) = eos.getPressure(2.7_f, 1.e5_f);
    REQUIRE(Math::almostEqual(p, 337500._f));
    REQUIRE(Math::almostEqual(cs, 9.88919808e9_f));

    tieToTuple(p, cs) = eos.getPressure(2.0_f, 1.e5_f);
    REQUIRE(Math::almostEqual(p, -5.12736563e9_f));
    REQUIRE(Math::almostEqual(cs, 2.99693619e9_f));

    tieToTuple(p, cs) = eos.getPressure(2.0_f, 1.e10_f);
    REQUIRE(Math::almostEqual(p, 9.34812365e9_f));
    REQUIRE(Math::almostEqual(cs, 4.52810138e9_f));

    tieToTuple(p, cs) = eos.getPressure(2.8_f, 1.e10_f);
    REQUIRE(Math::almostEqual(p, 1.50259651e10));
    REQUIRE(Math::almostEqual(cs, 1.83050916e10));

    tieToTuple(p, cs) = eos.getPressure(2.6_f, 1.e7_f);
    REQUIRE(Math::almostEqual(p, -883133952._f));
    REQUIRE(Math::almostEqual(cs, 7.89542707e9_f));

    tieToTuple(p, cs) = eos.getPressure(2.8_f, 1.e7_f);
    REQUIRE(Math::almostEqual(p, 1.03996064e9_f));
    REQUIRE(Math::almostEqual(cs, 1.08126444e10_f));
}
