#include "sph/solvers/AsymmetricSolver.h"
#include "catch.hpp"
#include "quantities/Iterate.h"
#include "sph/solvers/StandardSets.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "tests/Setup.h"
#include "timestepping/TimeStepping.h"
#include "utils/Utils.h"
#include <iostream>

using namespace Sph;

static Storage compute(const RunSettings& settings) {
    BodySettings body;
    body.set(BodySettingsId::EOS, EosEnum::IDEAL_GAS)
        .set(BodySettingsId::RHEOLOGY_DAMAGE, FractureEnum::NONE)
        .set(BodySettingsId::RHEOLOGY_YIELDING, YieldingEnum::NONE)
        .set(BodySettingsId::DENSITY, 100._f)
        .set(BodySettingsId::ENERGY, 100._f);

    SharedPtr<Storage> storage = makeShared<Storage>(Tests::getGassStorage(1000, body, 2._f));
    // randomize smoothing lenghts
    UniformRng rng;
    ArrayView<Vector> r = storage->getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        const Float mult = 0.1_f + 2._f * rng();
        r[i][H] *= mult;
    }


    SharedPtr<IScheduler> scheduler = Factory::getScheduler(settings);
    AsymmetricSolver solver(*scheduler, settings, getStandardEquations(settings));
    solver.create(*storage, storage->getMaterial(0));
    MaterialInitialContext context(settings);
    storage->getMaterial(0)->create(*storage, context);

    PredictorCorrector stepper(storage, settings);
    Statistics stats;
    for (Float t = 0._f; t < 0.01_f; t += stepper.getTimeStep()) {
        std::cout << "t = " << t << std::endl;
        stepper.step(*scheduler, solver, stats);
    }
    return std::move(*storage);
}

/// Solver should give EXACTLY the same results when SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP is used,
/// it should only affect the performance
TEST_CASE("AsymmetricSolver radii hash map", "[solvers]") {
    RunSettings settings;
    settings.set(RunSettingsId::SPH_SOLVER_TYPE, SolverEnum::ASYMMETRIC_SOLVER)
        .set(RunSettingsId::SPH_SOLVER_FORCES, ForceEnum::PRESSURE)
        .set(RunSettingsId::SPH_STRAIN_RATE_CORRECTION_TENSOR, false)
        .set(RunSettingsId::SPH_SUM_ONLY_UNDAMAGED, false);

    settings.set(RunSettingsId::SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP, false);
    Storage sim1 = compute(settings);

    settings.set(RunSettingsId::SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP, true);
    Storage sim2 = compute(settings);
    bool match = true;
    iteratePair<VisitorEnum::ALL_BUFFERS>(
        sim1, sim2, [&match](auto& ar1, auto& ar2) { match &= (ar1 == ar2); });
    REQUIRE(match);
}
