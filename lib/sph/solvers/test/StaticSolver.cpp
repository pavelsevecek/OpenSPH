#include "sph/solvers/StaticSolver.h"
#include "catch.hpp"
#include "physics/Constants.h"
#include "sph/equations/Potentials.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

#include "io/Logger.h"

using namespace Sph;

TEST_CASE("StaticSolver gravity vs. pressure", "[staticsolver]") {
    RunSettings settings;
    const Float rho0 = 1._f;
    auto potential = makeExternalForce([rho0](const Vector& pos) {
        const Float r = getLength(pos);
        return -Constants::gravity * rho0 * sphereVolume(r) * pos / pow<3>(r);
    });
    EquationHolder equations(std::move(potential));
    StaticSolver solver(settings, std::move(equations));

    BodySettings body;
    // zero shear modulus to get only pressure without other components of the stress tensor
    body.set(BodySettingsId::SHEAR_MODULUS, 0._f);
    Storage storage = Tests::getGassStorage(1000, body, 1._f * Constants::au, rho0);
    solver.create(storage, storage.getMaterial(0));

    Statistics stats;
    REQUIRE(solver.solve(storage, stats));

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> p = storage.getValue<Float>(QuantityId::PRESSURE);

    Float K = 0._f;
    auto expected = [&](const Float x) { //
        return K + 2._f / 3._f * PI * Constants::gravity * sqr(rho0) * sqr(x);
    };
    // find offset
    /// \todo this is extra step, we should specify boundary conditions
    Float offset = 0._f;
    Size cnt = 0;
    for (Size i = 0; i < r.size(); ++i) {
        if (getLength(r[i]) < 0.7_f * Constants::au) {
            offset += p[i] - expected(getLength(r[i]));
            cnt++;
        }
    }
    K = offset / cnt;

    auto test = [&](const Size i) -> Outcome { //
        if (getLength(r[i]) > 0.7_f * Constants::au) {
            return SUCCESS;
        }
        const Float p0 = expected(getLength(r[i]));
        if (p[i] != approx(p0, 0.05_f)) {
            return makeFailed("Incorrect pressure: \n", p[i], " == ", p0);
        }
        return SUCCESS;
    };

    REQUIRE_SEQUENCE(test, 0, r.size());

    FileLogger logger("p.txt");
    for (Size i = 0; i < r.size(); ++i) {
        logger.write(getLength(r[i]), "  ", p[i]);
    }
}
