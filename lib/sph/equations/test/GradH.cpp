#include "sph/equations/GradH.h"
#include "catch.hpp"
#include "tests/Setup.h"

using namespace Sph;

TEST_CASE("GradH", "[solvers]") {
    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults());
    EquationHolder eqs;
    RunSettings settings;
    settings.set(RunSettingsId::MODEL_FORCE_SOLID_STRESS, false);
    eqs += makeTerm<PressureForce>(settings) + makeTerm<ContinuityEquation>(settings) + makeTerm<GradH>() +
           makeTerm<ConstSmoothingLength>();
    GenericSolver solver(RunSettings::getDefaults(), std::move(eqs));
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}
