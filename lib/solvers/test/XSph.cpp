#include "solvers/XSph.h"
#include "catch.hpp"
#include "utils/Setup.h"

using namespace Sph;

TEST_CASE("XSph", "[solvers]") {
    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults());
    EquationHolder eqs;
    RunSettings settings;
    eqs += makeTerm<PressureForce>() + makeTerm<ContinuityEquation>();
    eqs += makeTerm<XSph>();
    GenericSolver solver(RunSettings::getDefaults(), std::move(eqs));
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}
