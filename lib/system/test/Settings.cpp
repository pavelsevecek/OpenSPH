#include "system/Settings.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Settings", "[settings]") {
    Optional<Float> rho = BODY_SETTINGS.get<Float>(BodySettingsIds::DENSITY);
    REQUIRE(rho);
    REQUIRE(rho.get() == 2700._f);

    Optional<int> n = BODY_SETTINGS.get<int>(BodySettingsIds::PARTICLE_COUNT);
    REQUIRE(n);
    REQUIRE(n.get() == 10000);
}
