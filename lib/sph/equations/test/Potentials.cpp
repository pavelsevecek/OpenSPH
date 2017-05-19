#include "sph/equations/Potentials.h"
#include "catch.hpp"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;


TEST_CASE("SphericalGravity consistency", "[equationterm]") {
    const Float rho0 = 100._f;
    Storage storage = Tests::getGassStorage(10000, BodySettings::getDefaults(), Constants::au, rho0);
    SphericalGravity gravity;
    // normally we would have to call create and initialize first, but they are empty for SphericalGravity
    gravity.finalize(storage);

    Storage expected = Tests::getGassStorage(10000, BodySettings::getDefaults(), Constants::au, rho0);
    auto potential = makeExternalForce([rho0](const Vector& pos) {
        const Float r = getLength(pos);
        return -Constants::gravity * rho0 * sphereVolume(r) * pos / pow<3>(r);
    });
    potential->finalize(expected);

    ArrayView<const Vector> dv1 = storage.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<const Vector> dv2 = expected.getD2t<Vector>(QuantityId::POSITIONS);
    auto test = [&](const Size i) { //
        return makeOutcome(
            dv1[i] == approx(dv2[i], 1.e-2_f), "invalid acceleration:\n", dv1[i], " == ", dv2[i]);
    };
    REQUIRE(dv1.size() > 500); // sanity check
    REQUIRE_SEQUENCE(test, 0, dv1.size());
}
