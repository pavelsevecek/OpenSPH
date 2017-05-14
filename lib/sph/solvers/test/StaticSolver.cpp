#include "sph/solvers/StaticSolver.h"
#include "catch.hpp"
#include "physics/Constants.h"
#include "sph/equations/Potentials.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;

TEST_CASE("StaticSolver gravity", "[staticsolver]") {
    RunSettings settings;
    const Float rho0 = 1000._f;
    auto potential = makeExternalForce([rho0](const Vector& pos) {
        const Float r = getLength(pos);
        return -Constants::gravity * rho0 * sphereVolume(r) * pos / pow<3>(r);
    });
    EquationHolder equations(std::move(potential));
    StaticSolver solver(settings, std::move(equations));

    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults(), 5._f * Constants::au, rho0);
    solver.create(storage, storage.getMaterial(0));

    Statistics stats;
    REQUIRE_NOTHROW(solver.solve(storage, stats));
}
