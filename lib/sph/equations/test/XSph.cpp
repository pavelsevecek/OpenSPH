#include "sph/equations/XSph.h"
#include "catch.hpp"
#include "sph/solvers/SymmetricSolver.h"
#include "system/Statistics.h"
#include "tests/Setup.h"

using namespace Sph;

TEST_CASE("XSph", "[solvers]") {

    /// \todo add proper tests, checking that velocities are indeed smoothed

    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults());
    EquationHolder eqs;
    RunSettings settings;
    settings.setFlags(RunSettingsId::SOLVER_FORCES, ForceEnum::PRESSURE);

    eqs += makeTerm<PressureForce>() + makeTerm<ContinuityEquation>() + makeTerm<XSph>() +
           makeTerm<ConstSmoothingLength>();
    SymmetricSolver solver(settings, std::move(eqs));
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));
}
