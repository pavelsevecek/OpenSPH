#include "system/Settings.h"
#include "catch.hpp"
#include "io/Path.h"
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

TEST_CASE("Settings save/load", "[settings]") {
    RunSettings settings;
    settings.set(RunSettingsId::DOMAIN_CENTER, Vector(1._f, 2._f, 3._f));
    settings.set(RunSettingsId::DOMAIN_RADIUS, 3.5_f);
    settings.set(RunSettingsId::RUN_NAME, std::string("test"));
    settings.saveToFile(Path("tmp.sph"));

    Settings<RunSettingsId> loadedSettings;
    const Outcome result = loadedSettings.loadFromFile(Path("tmp.sph"));
    REQUIRE(result);
    const Vector center = loadedSettings.get<Vector>(RunSettingsId::DOMAIN_CENTER);
    REQUIRE(center == Vector(1._f, 2._f, 3._f));
    const Float radius = loadedSettings.get<Float>(RunSettingsId::DOMAIN_RADIUS);
    REQUIRE(radius == 3.5_f);
    const std::string name = loadedSettings.get<std::string>(RunSettingsId::RUN_NAME);
    REQUIRE(name == "test");

    REQUIRE_FALSE(loadedSettings.loadFromFile(Path("nonexistingFile.sph")));
}

TEST_CASE("Settings iterator", "[settings]") {
    RunSettings settings(EMPTY_SETTINGS);
    settings.set(RunSettingsId::DOMAIN_CENTER, Vector(1._f, 2._f, 3._f));
    settings.set(RunSettingsId::DOMAIN_RADIUS, 3.5_f);
    settings.set(RunSettingsId::RUN_NAME, std::string("test"));
    REQUIRE(settings.size() == 3);

    // Rb tree sorts the entries
    auto iter = settings.begin();
    REQUIRE((*iter).id == RunSettingsId::RUN_NAME);
    REQUIRE((*iter).value.get<std::string>() == std::string("test"));
    ++iter;
    REQUIRE((*iter).id == RunSettingsId::DOMAIN_CENTER);
    REQUIRE((*iter).value.get<Vector>() == Vector(1._f, 2._f, 3._f));
    ++iter;
    REQUIRE((*iter).id == RunSettingsId::DOMAIN_RADIUS);
    REQUIRE((*iter).value.get<Float>() == 3.5_f);
    ++iter;
    REQUIRE(iter == settings.end());
}
