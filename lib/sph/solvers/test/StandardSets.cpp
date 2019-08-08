#include "sph/solvers/StandardSets.h"
#include "catch.hpp"
#include "quantities/Quantity.h"
#include "sph/solvers/AsymmetricSolver.h"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Statistics.h"
#include "tests/Setup.h"
#include "timestepping/TimeStepping.h"
#include "utils/SequenceTest.h"
#include "utils/Utils.h"

using namespace Sph;

template <typename TSolver>
void testSolver(Storage& storage, const RunSettings& settings) {
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    TSolver solver(pool, settings, getStandardEquations(settings));
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}

template <typename TSolver>
static Storage initStorage(TSolver& solver, BodySettings body) {
    Storage storage = Tests::getSolidStorage(10, body);
    solver.create(storage, storage.getMaterial(0));
    return storage;
}

TEMPLATE_TEST_CASE("StandardSets quantities B&A", "[solvers]", SymmetricSolver, AsymmetricSolver) {
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    settings.set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::BENZ_ASPHAUG);
    settings.set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONTINUITY_EQUATION);
    TestType solver(pool, settings, getStandardEquations(settings));

    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    Storage storage = initStorage(solver, body);
    // positions, masses, pressure, density, energy, sound speed, deviatoric stress, yielding reduction,
    // velocity divergence, velocity gradient, neighbour count, flags, material count
    REQUIRE(storage.getQuantityCnt() == 13);
    REQUIRE(storage.has<Vector>(QuantityId::POSITION, OrderEnum::SECOND));
    REQUIRE(storage.has<Float>(QuantityId::MASS, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::PRESSURE, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::DENSITY, OrderEnum::FIRST));
    REQUIRE(storage.has<Float>(QuantityId::ENERGY, OrderEnum::FIRST));
    REQUIRE(storage.has<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO));
    REQUIRE(storage.has<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::FIRST));
    REQUIRE(storage.has<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO));
    REQUIRE(storage.has<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO));
    REQUIRE(storage.has<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO));
    REQUIRE(storage.has<Size>(QuantityId::FLAG, OrderEnum::ZERO));
    REQUIRE(storage.has<Size>(QuantityId::MATERIAL_ID, OrderEnum::ZERO));

    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP);
    storage = initStorage(solver, body);
    REQUIRE(storage.getQuantityCnt() == 18);
    REQUIRE(storage.has<Float>(QuantityId::DAMAGE, OrderEnum::FIRST));
    REQUIRE(storage.has<Float>(QuantityId::EPS_MIN, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::M_ZERO, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::EXPLICIT_GROWTH, OrderEnum::ZERO));
    REQUIRE(storage.has<Size>(QuantityId::N_FLAWS, OrderEnum::ZERO));
}

TEMPLATE_TEST_CASE("StandardSets quantities standard", "[solvers]", SymmetricSolver, AsymmetricSolver) {
    RunSettings settings;
    settings.set(RunSettingsId::SPH_DISCRETIZATION, DiscretizationEnum::STANDARD);
    settings.set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONTINUITY_EQUATION);

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    TestType solver(pool, settings, getStandardEquations(settings));

    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    Storage storage = initStorage(solver, body);
    // positions, masses, pressure, density, energy, sound speed, deviatoric stress, yielding reduction,
    // density velocity divergence, neighbour count, flags, material count
    REQUIRE(storage.getQuantityCnt() == 13);
    REQUIRE(storage.has<Vector>(QuantityId::POSITION, OrderEnum::SECOND));
    REQUIRE(storage.has<Float>(QuantityId::MASS, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::PRESSURE, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::DENSITY, OrderEnum::FIRST));
    REQUIRE(storage.has<Float>(QuantityId::ENERGY, OrderEnum::FIRST));
    REQUIRE(storage.has<Float>(QuantityId::SOUND_SPEED, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::STRESS_REDUCING, OrderEnum::ZERO));
    REQUIRE(storage.has<TracelessTensor>(QuantityId::DEVIATORIC_STRESS, OrderEnum::FIRST));
    REQUIRE(storage.has<SymmetricTensor>(QuantityId::VELOCITY_GRADIENT, OrderEnum::ZERO));
    REQUIRE(storage.has<Float>(QuantityId::VELOCITY_DIVERGENCE, OrderEnum::ZERO));
    REQUIRE(storage.has<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO));
    REQUIRE(storage.has<Size>(QuantityId::FLAG, OrderEnum::ZERO));
    REQUIRE(storage.has<Size>(QuantityId::MATERIAL_ID, OrderEnum::ZERO));

    // damage is the same in both formulations
}

TEMPLATE_TEST_CASE("StandardSets gass", "[solvers]", SymmetricSolver, AsymmetricSolver) {
    RunSettings settings;
    settings.set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE);
    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::NONE);
    settings.set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 1._f)
        .set(BodySettingsId::ENERGY, 1._f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    Storage storage;

    storage = Tests::getGassStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);

    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD);
    // recreate storage, solver needs to re-create the quantities (would assert otherwise)
    storage = Tests::getGassStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);

    settings.set(RunSettingsId::SPH_AV_USE_BALSARA, true);
    storage = Tests::getGassStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);

    settings.set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONTINUITY_EQUATION);
    storage = Tests::getGassStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);

    settings.set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH,
        SmoothingLengthEnum::CONTINUITY_EQUATION | SmoothingLengthEnum::SOUND_SPEED_ENFORCING);
    storage = Tests::getGassStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);
}

TEMPLATE_TEST_CASE("StandardSets solid", "[solvers]", SymmetricSolver, AsymmetricSolver) {
    RunSettings settings;
    settings.set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS);
    settings.set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);
    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::NONE);
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    Storage storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);

    /// \todo this probably won't apply damage as it uses some dummy rheology, but it shouldn't throw
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP);
    storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);

    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);

    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);

    settings.set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH,
        SmoothingLengthEnum::CONTINUITY_EQUATION | SmoothingLengthEnum::SOUND_SPEED_ENFORCING);
    storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver<TestType>(storage, settings);
}

TEMPLATE_TEST_CASE("StandardSets constant smoothing length", "[solvers]", SymmetricSolver, AsymmetricSolver) {
    // there was a bug that smoothing length changed (incorrectly) for SmoothingLengthEnum::CONST

    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getSolidStorage(10000));
    RunSettings settings;
    settings.set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS);
    settings.set(RunSettingsId::SPH_ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    TestType solver(pool, settings, getStandardEquations(settings));
    solver.create(*storage, storage->getMaterial(0));

    // setup nonzero velocities
    ArrayView<Vector> v = storage->getDt<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < v.size(); ++i) {
        while (getLength(v[i]) < EPS) {
            v[i] = randomVector();
        }
    }

    Array<Vector> initialPositions = storage->getValue<Vector>(QuantityId::POSITION).clone();

    EulerExplicit timestepping(storage, settings);
    Statistics stats;
    timestepping.step(pool, solver, stats);
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);

    auto test = [&](const Size i) -> Outcome {
        if (r[i] == initialPositions[i]) {
            return "Particle didn't move";
        }
        if (r[i][H] != initialPositions[i][H]) {
            return "Smoothing length CHANGED!";
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}
