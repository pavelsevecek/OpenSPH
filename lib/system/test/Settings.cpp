#include "system/Settings.h"
#include "catch.hpp"

using namespace Sph;

TEST_CASE("Settings set/get", "[settings]") {
    Settings<BodySettingsIds> settings;
    settings.set(BodySettingsIds::DENSITY, 100._f);
    settings.set(BodySettingsIds::PARTICLE_COUNT, 50);
    settings.set(BodySettingsIds::DENSITY_RANGE, Range(1._f, 2._f));

    Float rho = settings.get<Float>(BodySettingsIds::DENSITY);
    REQUIRE(rho == 100._f);

    int n = settings.get<int>(BodySettingsIds::PARTICLE_COUNT);
    REQUIRE(n == 50);

    Range range = settings.get<Range>(BodySettingsIds::DENSITY_RANGE);
    REQUIRE(range == Range(1._f, 2._f));
}

TEST_CASE("Settings save/load", "[settings]") {
    Settings<GlobalSettingsIds> settings(GLOBAL_SETTINGS); // needed to copy names
    settings.set(GlobalSettingsIds::DOMAIN_CENTER, Vector(1._f, 2._f, 3._f));
    settings.set(GlobalSettingsIds::DOMAIN_RADIUS, 3.5f);
    settings.set(GlobalSettingsIds::RUN_NAME, std::string("test"));
    settings.saveToFile("tmp.sph");

    Settings<GlobalSettingsIds> loadedSettings;
    REQUIRE(loadedSettings.loadFromFile("tmp.sph", GLOBAL_SETTINGS));
    const Vector center = loadedSettings.get<Vector>(GlobalSettingsIds::DOMAIN_CENTER);
    REQUIRE(center == Vector(1._f, 2._f, 3._f));
    const Float radius = loadedSettings.get<float>(GlobalSettingsIds::DOMAIN_RADIUS);
    REQUIRE(radius == 3.5f);
    const std::string name = loadedSettings.get<std::string>(GlobalSettingsIds::RUN_NAME);
    REQUIRE(name == "test");
}
