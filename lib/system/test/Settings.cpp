#include "system/Settings.h"
#include "catch.hpp"
#include "io/Logger.h"
#include "objects/wrappers/Outcome.h"

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

TEST_CASE("Settings copyValueTo", "[settings]") {
    Settings<BodySettingsIds> settings1, settings2;
    settings1.set(BodySettingsIds::DENSITY, 100._f);
    settings1.set(BodySettingsIds::PARTICLE_COUNT, 10);
    settings1.set(BodySettingsIds::DAMAGE_RANGE, Range(2._f, 6._f));
    settings1.set(BodySettingsIds::EOS, std::string("string"));

    settings1.copyValueTo(BodySettingsIds::DENSITY, settings2);
    REQUIRE(settings2.get<Float>(BodySettingsIds::DENSITY) == 100._f);

    settings1.copyValueTo(BodySettingsIds::EOS, settings2);
    REQUIRE(settings2.get<std::string>(BodySettingsIds::EOS) == "string");
}

TEST_CASE("Settings save/load", "[settings]") {
    GlobalSettings settings;
    settings.set(GlobalSettingsIds::DOMAIN_CENTER, Vector(1._f, 2._f, 3._f));
    settings.set(GlobalSettingsIds::DOMAIN_RADIUS, 3.5_f);
    settings.set(GlobalSettingsIds::RUN_NAME, std::string("test"));
    settings.saveToFile("tmp.sph");

    Settings<GlobalSettingsIds> loadedSettings;
    const Outcome result = loadedSettings.loadFromFile("tmp.sph", GlobalSettings::getDefaults());
    if (!result) {
        StdOutLogger().write(result.error());
    }
    REQUIRE(result);
    const Vector center = loadedSettings.get<Vector>(GlobalSettingsIds::DOMAIN_CENTER);
    REQUIRE(center == Vector(1._f, 2._f, 3._f));
    const Float radius = loadedSettings.get<Float>(GlobalSettingsIds::DOMAIN_RADIUS);
    REQUIRE(radius == 3.5_f);
    const std::string name = loadedSettings.get<std::string>(GlobalSettingsIds::RUN_NAME);
    REQUIRE(name == "test");
}
