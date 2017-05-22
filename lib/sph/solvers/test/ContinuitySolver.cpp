#include "sph/solvers/ContinuitySolver.h"
#include "catch.hpp"
#include "utils/Setup.h"

using namespace Sph;

void testSolver(Storage& storage, const RunSettings& settings) {
    ContinuitySolver solver(settings);
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}

TEST_CASE("ContinuitySolver gass", "[solvers]") {
    RunSettings settings;
    settings.set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, false);
    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::NONE);
    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    Storage storage = Tests::getGassStorage(100, body, 1._f);
    testSolver(storage, settings);

    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD);
    testSolver(storage, settings);

    settings.set(RunSettingsId::SPH_AV_BALSARA, true);
    testSolver(storage, settings);

    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONTINUITY_EQUATION);
    testSolver(storage, settings);

    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH,
        int(SmoothingLengthEnum::CONTINUITY_EQUATION) | int(SmoothingLengthEnum::SOUND_SPEED_ENFORCING));
    testSolver(storage, settings);
}

TEST_CASE("ContinuitySolver solid", "[solvers]") {
    RunSettings settings;
    settings.set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, true);
    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);
    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::NONE);
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    Storage storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver(storage, settings);

    /// \todo this probably won't apply damage as it uses some dummy rheology, but it shouldn't throw
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP);
    storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver(storage, settings);

    body.set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver(storage, settings);

    body.set(BodySettingsId::RHEOLOGY_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    testSolver(storage, settings);
    testSolver(storage, settings);

    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH,
        int(SmoothingLengthEnum::CONTINUITY_EQUATION) | int(SmoothingLengthEnum::SOUND_SPEED_ENFORCING));
    testSolver(storage, settings);
}
