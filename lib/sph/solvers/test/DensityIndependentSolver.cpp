#include "sph/solvers/DensityIndependentSolver.h"
#include "catch.hpp"
#include "tests/Setup.h"

using namespace Sph;

TEST_CASE("DensityIndependentSolver", "[solvers]") {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 1._f)
        .set(BodySettingsId::ENERGY, 1._f)
        .set(BodySettingsId::ENERGY_RANGE, Interval(1._f, INFTY));
    Storage storage = Tests::getGassStorage(1000, settings);
    DensityIndependentSolver solver(RunSettings::getDefaults());
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}
