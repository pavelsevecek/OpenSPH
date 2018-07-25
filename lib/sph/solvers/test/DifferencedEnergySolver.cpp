#include "sph/solvers/DifferencedEnergySolver.h"
#include "catch.hpp"
#include "sph/solvers/StandardSets.h"
#include "tests/Setup.h"

using namespace Sph;

TEST_CASE("DifferencedEnergySolver", "[solvers]") {
    RunSettings settings;
    BodySettings body;
    body.set(BodySettingsId::DENSITY, 1._f)
        .set(BodySettingsId::ENERGY, 1._f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(1._f, INFTY));
    Storage storage = Tests::getSolidStorage(1000, body);
    ThreadPool& pool = *ThreadPool::getGlobalInstance();
    DifferencedEnergySolver solver(pool, settings, getStandardEquations(settings));
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    stats.set(StatisticsId::TIMESTEP_VALUE, 1._f);
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}
