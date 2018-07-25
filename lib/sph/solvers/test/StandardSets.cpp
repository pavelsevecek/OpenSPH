#include "sph/solvers/StandardSets.h"
#include "catch.hpp"
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

TYPED_TEST_CASE_2("StandardSets quantities B&A", "[solvers]", TSolver, SymmetricSolver, AsymmetricSolver) {
    RunSettings settings;
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    settings.set(RunSettingsId::SPH_FORMULATION, FormulationEnum::BENZ_ASPHAUG);
    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONTINUITY_EQUATION);
    TSolver solver(pool, settings, getStandardEquations(settings));

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

TYPED_TEST_CASE_2("StandardSets quantities standard",
    "[solvers]",
    TSolver,
    SymmetricSolver,
    AsymmetricSolver) {
    RunSettings settings;
    settings.set(RunSettingsId::SPH_FORMULATION, FormulationEnum::STANDARD);
    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONTINUITY_EQUATION);

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    TSolver solver(pool, settings, getStandardEquations(settings));

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

TYPED_TEST_CASE_2("StandardSets gass", "[solvers]", TSolver, SymmetricSolver, AsymmetricSolver) {
    RunSettings settings;
    settings.set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE);
    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::NONE);
    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 1._f)
        .set(BodySettingsId::ENERGY, 1._f)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    Storage storage = Tests::getGassStorage(100, body, 1._f);
    testSolver<TSolver>(storage, settings);

    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::STANDARD);
    testSolver<TSolver>(storage, settings);

    settings.set(RunSettingsId::SPH_AV_BALSARA, true);
    testSolver<TSolver>(storage, settings);

    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONTINUITY_EQUATION);
    testSolver<TSolver>(storage, settings);

    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH,
        SmoothingLengthEnum::CONTINUITY_EQUATION | SmoothingLengthEnum::SOUND_SPEED_ENFORCING);
    testSolver<TSolver>(storage, settings);
}

TYPED_TEST_CASE_2("StandardSets solid", "[solvers]", TSolver, SymmetricSolver, AsymmetricSolver) {
    RunSettings settings;
    settings.set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS);
    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);
    settings.set(RunSettingsId::SPH_AV_TYPE, ArtificialViscosityEnum::NONE);
    BodySettings body;
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE);
    Storage storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver<TSolver>(storage, settings);

    /// \todo this probably won't apply damage as it uses some dummy rheology, but it shouldn't throw
    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP);
    storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver<TSolver>(storage, settings);

    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    storage = Tests::getSolidStorage(100, body, 1._f);
    testSolver<TSolver>(storage, settings);

    body.set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::SCALAR_GRADY_KIPP);
    body.set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::VON_MISES);
    testSolver<TSolver>(storage, settings);
    testSolver<TSolver>(storage, settings);

    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH,
        SmoothingLengthEnum::CONTINUITY_EQUATION | SmoothingLengthEnum::SOUND_SPEED_ENFORCING);
    testSolver<TSolver>(storage, settings);
}

TYPED_TEST_CASE_2("StandardSets constant smoothing length",
    "[solvers]",
    TSolver,
    SymmetricSolver,
    AsymmetricSolver) {
    // there was a bug that smoothing length changed (incorrectly) for SmoothingLengthEnum::CONST

    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getSolidStorage(10000));
    RunSettings settings;
    settings.set(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE | ForceEnum::SOLID_STRESS);
    settings.set(RunSettingsId::ADAPTIVE_SMOOTHING_LENGTH, SmoothingLengthEnum::CONST);

    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    TSolver solver(pool, settings, getStandardEquations(settings));
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
