#include "system/Settings.h"
#include "catch.hpp"
#include "io/Path.h"
#include "objects/wrappers/Outcome.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Settings set/get", "[settings]") {
    Settings<BodySettingsId> settings;
    settings.set(BodySettingsId::DENSITY, 100._f);
    settings.set(BodySettingsId::PARTICLE_COUNT, 50);
    settings.set(BodySettingsId::DENSITY_RANGE, Interval(1._f, 2._f));

    Float rho = settings.get<Float>(BodySettingsId::DENSITY);
    REQUIRE(rho == 100._f);

    int n = settings.get<int>(BodySettingsId::PARTICLE_COUNT);
    REQUIRE(n == 50);

    Interval range = settings.get<Interval>(BodySettingsId::DENSITY_RANGE);
    REQUIRE(range == Interval(1._f, 2._f));

    REQUIRE_ASSERT(settings.get<int>(BodySettingsId::DENSITY_RANGE));
}

TEST_CASE("Settings has", "[settings]") {
    RunSettings settings;
    REQUIRE(settings.has<Float>(RunSettingsId::COLLISION_ALLOWED_OVERLAP));
    REQUIRE(settings.has<bool>(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR));
    REQUIRE(settings.has<std::string>(RunSettingsId::RUN_NAME));
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

TEST_CASE("Settings enums", "[settings]") {
    RunSettings settings(EMPTY_SETTINGS);
    settings.set(RunSettingsId::RUN_OUTPUT_TYPE, OutputEnum::BINARY_FILE);
    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION,
        TimeStepCriterionEnum::COURANT | TimeStepCriterionEnum::ACCELERATION);

    REQUIRE(settings.get<OutputEnum>(RunSettingsId::RUN_OUTPUT_TYPE) == OutputEnum::BINARY_FILE);
    REQUIRE(settings.get<SmoothingLengthEnum>(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH) ==
            SmoothingLengthEnum::CONST);
    Flags<TimeStepCriterionEnum> flags =
        settings.getFlags<TimeStepCriterionEnum>(RunSettingsId::TIMESTEPPING_CRITERION);
    REQUIRE(flags == (TimeStepCriterionEnum::COURANT | TimeStepCriterionEnum::ACCELERATION));
    REQUIRE_ASSERT(settings.get<int>(RunSettingsId::RUN_OUTPUT_TYPE));
    REQUIRE_ASSERT(settings.get<SmoothingLengthEnum>(RunSettingsId::RUN_OUTPUT_TYPE));
}

TEST_CASE("Settings save/load basic", "[settings]") {
    RunSettings settings;
    settings.set(RunSettingsId::DOMAIN_CENTER, Vector(1._f, 2._f, 3._f));
    settings.set(RunSettingsId::DOMAIN_RADIUS, 3.5_f);
    settings.set(RunSettingsId::RUN_NAME, std::string("test"));
    settings.set(RunSettingsId::RUN_OUTPUT_TYPE, OutputEnum::BINARY_FILE);
    settings.saveToFile(Path("tmp1.sph"));

    RunSettings loadedSettings;
    const Outcome result = loadedSettings.loadFromFile(Path("tmp1.sph"));
    REQUIRE(result);
    const Vector center = loadedSettings.get<Vector>(RunSettingsId::DOMAIN_CENTER);
    REQUIRE(center == Vector(1._f, 2._f, 3._f));
    const Float radius = loadedSettings.get<Float>(RunSettingsId::DOMAIN_RADIUS);
    REQUIRE(radius == 3.5_f);
    const std::string name = loadedSettings.get<std::string>(RunSettingsId::RUN_NAME);
    REQUIRE(name == "test");
    const OutputEnum output = loadedSettings.get<OutputEnum>(RunSettingsId::RUN_OUTPUT_TYPE);
    REQUIRE(output == OutputEnum::BINARY_FILE);

    REQUIRE_FALSE(loadedSettings.loadFromFile(Path("nonexistingFile.sph")));

    BodySettings body;
    // just test that body settings also work without asserts, otherwise the system is the same
    REQUIRE_NOTHROW(body.saveToFile(Path("tmp2.sph")));
}

TEST_CASE("Settings save/load flags", "[settings]") {
    RunSettings settings;
    settings.set(RunSettingsId::TIMESTEPPING_CRITERION,
        TimeStepCriterionEnum::COURANT | TimeStepCriterionEnum::ACCELERATION);
    settings.saveToFile(Path("tmp3.sph"));
    RunSettings loadedSettings;
    REQUIRE(loadedSettings.loadFromFile(Path("tmp3.sph")));
    Flags<TimeStepCriterionEnum> flags =
        loadedSettings.getFlags<TimeStepCriterionEnum>(RunSettingsId::TIMESTEPPING_CRITERION);
    REQUIRE(flags == (TimeStepCriterionEnum::COURANT | TimeStepCriterionEnum::ACCELERATION));
}

template <typename T>
static bool areValuesEqual(const T& t1, const T& t2) {
    return t1 == t2;
}
template <>
bool areValuesEqual(const Float& t1, const Float& t2) {
    return almostEqual(t1, t2, 1.e-4_f);
}

static bool areSettingsEqual(RunSettings& settings1, RunSettings& settings2) {
    SettingsIterator<RunSettingsId> iter1 = settings1.begin();
    SettingsIterator<RunSettingsId> iter2 = settings2.begin();
    for (; iter1 != settings1.end(); ++iter1, ++iter2) {
        SettingsIterator<RunSettingsId>::IteratorValue value1 = *iter1;
        SettingsIterator<RunSettingsId>::IteratorValue value2 = *iter2;
        if (value1.id != value2.id) {
            return false;
        }
        auto p1 = value1.value;
        auto p2 = value2.value;
        bool result = forValue(p1, [&p2](auto& v1) {
            using T2 = std::decay_t<decltype(v1)>;
            if (!p2.has<T2>()) {
                return false;
            }
            if (areValuesEqual(p2.get<T2>(), v1)) {
                return true;
            } else {
                StdOutLogger logger;
                logger.write(v1, " == ", p2.get<T2>());
                return false;
            }
        });
        if (!result) {
            return false;
        }
    }
    return true;
}

TEST_CASE("Settings save/load complete", "[settings]") {
    RunSettings settings1;
    settings1.set(RunSettingsId::DOMAIN_RADIUS, 3.5_f);
    settings1.set(RunSettingsId::RUN_NAME, std::string("lll"));
    settings1.set(RunSettingsId::TIMESTEPPING_CRITERION, EMPTY_FLAGS);
    settings1.saveToFile(Path("tmp4.sph"));
    RunSettings settings2(EMPTY_SETTINGS);
    REQUIRE(settings2.loadFromFile(Path("tmp4.sph")));

    REQUIRE(settings1.size() == settings2.size());
    REQUIRE(areSettingsEqual(settings1, settings2));
}
