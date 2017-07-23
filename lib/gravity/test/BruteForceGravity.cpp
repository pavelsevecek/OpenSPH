#include "gravity/BruteForceGravity.h"
#include "catch.hpp"
#include "sph/equations/Potentials.h"
#include "tests/Setup.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("BruteForceGravity", "[gravity]") {
    BruteForceGravity gravity;
    SphericalGravity analytic(SphericalGravity::Options::ASSUME_HOMOGENEOUS);

    const Float r0 = 1.e7_f;
    const Float rho0 = 100.f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getGassStorage(1000, settings, r0);
    // compute analytical acceleraion
    analytic.finalize(storage);

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Vector> d2v = storage.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);
    gravity.build(r, m);

    Statistics stats;
    auto test = [&](const Size i) -> Outcome {
        const Vector a = gravity.eval(i, stats);
        // around origin the relative comparison is very imprecise, just skip
        if (getLength(r[i]) < 0.1 * r0) {
            return SUCCESS;
        }
        if (a != approx(d2v[i], 0.04_f)) {
            return makeFailed("Incorrect acceleration: ", a, " == ", d2v[i]);
        } else {
            return SUCCESS;
        }
    };

    REQUIRE_SEQUENCE(test, 0, r.size());
}
