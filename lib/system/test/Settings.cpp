#include "system/Settings.h"
#include "catch.hpp"
#include "io/Logger.h"
#include "objects/wrappers/Outcome.h"

using namespace Sph;

TEST_CASE("Settings set/get", "[settings]") {
    Settings<BodySettingsId> settings;
    settings.set(BodySettingsId::DENSITY, 100._f);
    settings.set(BodySettingsId::PARTICLE_COUNT, 50);
    settings.set(BodySettingsId::DENSITY_RANGE, Range(1._f, 2._f));

    Float rho = settings.get<Float>(BodySettingsId::DENSITY);
    REQUIRE(rho == 100._f);

    int n = settings.get<int>(BodySettingsId::PARTICLE_COUNT);
    REQUIRE(n == 50);

    Range range = settings.get<Range>(BodySettingsId::DENSITY_RANGE);
    REQUIRE(range == Range(1._f, 2._f));
}

TEST_CASE("Settings copyValueTo", "[settings]") {
    Settings<BodySettingsId> settings1, settings2;
    settings1.set(BodySettingsId::DENSITY, 100._f);
    settings1.set(BodySettingsId::PARTICLE_COUNT, 10);
    settings1.set(BodySettingsId::DAMAGE_RANGE, Range(2._f, 6._f));
    settings1.set(BodySettingsId::EOS, std::string("string"));

    settings1.copyValueTo(BodySettingsId::DENSITY, settings2);
    REQUIRE(settings2.get<Float>(BodySettingsId::DENSITY) == 100._f);

    settings1.copyValueTo(BodySettingsId::EOS, settings2);
    REQUIRE(settings2.get<std::string>(BodySettingsId::EOS) == "string");
}

TEST_CASE("Settings save/load", "[settings]") {
    RunSettings settings;
    settings.set(RunSettingsId::DOMAIN_CENTER, Vector(1._f, 2._f, 3._f));
    settings.set(RunSettingsId::DOMAIN_RADIUS, 3.5_f);
    settings.set(RunSettingsId::RUN_NAME, std::string("test"));
    settings.saveToFile("tmp.sph");

    Settings<RunSettingsId> loadedSettings;
    const Outcome result = loadedSettings.loadFromFile("tmp.sph", RunSettings::getDefaults());
    if (!result) {
        StdOutLogger().write(result.error());
    }
    REQUIRE(result);
    const Vector center = loadedSettings.get<Vector>(RunSettingsId::DOMAIN_CENTER);
    REQUIRE(center == Vector(1._f, 2._f, 3._f));
    const Float radius = loadedSettings.get<Float>(RunSettingsId::DOMAIN_RADIUS);
    REQUIRE(radius == 3.5_f);
    const std::string name = loadedSettings.get<std::string>(RunSettingsId::RUN_NAME);
    REQUIRE(name == "test");
}
