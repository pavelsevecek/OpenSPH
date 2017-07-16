#include "sph/solvers/GravitySolver.h"
#include "catch.hpp"
#include "tests/Setup.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("GravitySolver", "[solvers]") {
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 1._f).set(BodySettingsId::ENERGY, 1._f);
    Storage storage = Tests::getGassStorage(3000, settings, 100 * Constants::au);
    // no SPH equations, just gravity
    GravitySolver solver(RunSettings::getDefaults(), EquationHolder());
    REQUIRE_NOTHROW(solver.create(storage, storage.getMaterial(0)));
    Statistics stats;
    REQUIRE_NOTHROW(solver.integrate(storage, stats));

    // only gravity, no pressure -> gall cloud should collapse, acceleration to the center
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);

    auto test = [&](const Size i) -> Outcome {
        if (getLength(dv[i]) == 0._f) {
            return makeFailed("No acceleration for particle ", i);
        }
        if (getLength(r[i]) > EPS) {
            // check acceleration direction: dv ~ -r
            // avoid numerical issues for particles close to the center
            const Vector r0 = getNormalized(r[i]);
            const Vector dv0 = getNormalized(dv[i]);
            /// \todo this is quite imprecise, is it to be expected?
            if (dv0 != approx(-r0, 0.1_f)) {
                return makeFailed(
                    "Incorrect acceleration direction for particle ", i, "\n r0 = ", r0, " / dv0 = ", dv0);
            }
        }
        // check magnitude of acceleration
        const Float rho0 = storage.getMaterial(0)->getParam<Float>(BodySettingsId::DENSITY);
        const Float M = sphereVolume(getLength(r[i])) * rho0;
        const Float expected = Constants::gravity * M / getSqrLength(r[i]);
        const Float actual = getLength(dv[i]);
        /// \todo actual value seems to be under-estimated, is there some sort of discretization bias?
        if (actual != approx(expected, 0.1_f)) {
            return makeFailed(
                "Incorrect acceleration magnitude for particle ", i, "\n", actual, " == ", expected);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}
