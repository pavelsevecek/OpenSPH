#include "sph/equations/Potentials.h"
#include "catch.hpp"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"
#include "utils/Setup.h"

using namespace Sph;


TEST_CASE("SphericalGravity consistency", "[equationterm]") {
    Storage storage = Tests::getGassStorage(1000); // rho = 1
    SphericalGravity gravity;
    // normally we would have to call create and initialize first, but they are empty for SphericalGravity
    gravity.finalize(storage);

    Storage expected = Tests::getGassStorage(1000);
    auto potential = makeExternalForce([](const Vector& pos) {
        const Float r = getLength(pos);
        return -Constants::gravity * 1._f * sphereVolume(r) * pos / pow<3>(r);
    });
    potential->finalize(expected);

    ArrayView<const Vector> dv1 = storage.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<const Vector> dv2 = expected.getD2t<Vector>(QuantityId::POSITIONS);
    auto test = [&](const Size i) { //
        /// \todo figure out why is there such a huge discrepancy!
        return makeOutcome(dv1[i] == approx(dv2[i], 0.5_f * Constants::gravity),
            "invalid acceleration:\n",
            dv1[i],
            " == ",
            dv2[i]);
    };
    REQUIRE(dv1.size() > 500); // sanity check
    REQUIRE_SEQUENCE(test, 0, dv1.size());
}
