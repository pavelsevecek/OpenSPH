#include "sph/solvers/DensityIndependentSolver.h"
#include "catch.hpp"
#include "system/Statistics.h"
#include "tests/Setup.h"

using namespace Sph;

TEST_CASE("DensityIndependentSolver", "[solvers]") {
    RunSettings settings;
    settings.set(RunSettingsId::SPH_SUM_ONLY_UNDAMAGED, false);

    BodySettings body;
    body.set(BodySettingsId::DENSITY, 1._f)
        .set(BodySettingsId::ENERGY, 1._f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(1._f, INFTY));
    Storage storage = Tests::getGassStorage(1000, body);
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    DensityIndependentSolver solver(pool, settings);
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}
