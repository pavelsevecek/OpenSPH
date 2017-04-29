#include "solvers/DensityIndependentSolver.h"
#include "catch.hpp"
#include "utils/Setup.h"

using namespace Sph;

TEST_CASE("DensityIndependentSolver", "[solvers]") {
    Storage storage = Tests::getGassStorage(1000);
    DensityIndependentSolver solver(RunSettings::getDefaults());
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}
