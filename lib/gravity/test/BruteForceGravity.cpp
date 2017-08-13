#include "gravity/BruteForceGravity.h"
#include "catch.hpp"
#include "sph/equations/Potentials.h"
#include "tests/Setup.h"
#include "tests/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("BruteForceGravity single-thread", "[gravity]") {
    BruteForceGravity gravity;
    SphericalGravity analytic(SphericalGravity::Options::ASSUME_HOMOGENEOUS);

    const Float r0 = 1.e7_f;
    const Float rho0 = 100.f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getGassStorage(1000, settings, r0);
    // compute analytical acceleraion
    analytic.finalize(storage);
    Array<Vector> a = storage.getD2t<Vector>(QuantityId::POSITIONS).clone();

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Vector> d2v = storage.getD2t<Vector>(QuantityId::POSITIONS);
    storage.zeroHighestDerivatives(); // clear derivatives computed by analytic
    gravity.build(storage);

    Statistics stats;
    gravity.evalAll(d2v, stats);

    auto test = [&](const Size i) -> Outcome {
        // around origin the relative comparison is very imprecise, just skip
        if (getLength(r[i]) < 0.1 * r0) {
            return SUCCESS;
        }
        if (a[i] != approx(d2v[i], 0.04_f)) {
            return makeFailed("Incorrect acceleration: ", a[i], " == ", d2v[i]);
        } else {
            return SUCCESS;
        }
    };

    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("BruteForceGravity parallel", "[gravity]") {
    const Float r0 = 1.e7_f;
    const Float rho0 = 100.f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getGassStorage(1000, settings, r0);

    BruteForceGravity gravity;
    gravity.build(storage);
    Array<Vector> dv1 = storage.getD2t<Vector>(QuantityId::POSITIONS).clone();
    Statistics stats;
    gravity.evalAll(dv1, stats);

    ThreadPool pool;
    Array<Vector> dv2(dv1.size());
    dv2.fill(Vector(0._f));
    // evaluate gravity using parallel implementation
    gravity.evalAll(pool, dv2, stats);

    // compare with single-threaded result
    auto test = [&](const Size i) -> Outcome {
        if (dv2[i] != dv1[i]) {
            return makeFailed("Incorrect acceleration: ", dv2[i], " == ", dv1[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, dv1.size());
}
