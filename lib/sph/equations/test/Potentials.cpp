#include "sph/equations/Potentials.h"
#include "catch.hpp"
#include "tests/Setup.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;


TEST_CASE("SphericalGravity consistency", "[equationterm]") {
    BodySettings settings;
    const Float rho0 = 100._f;
    settings.set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getGassStorage(1000, settings, Constants::au);
    SphericalGravity gravity1(EMPTY_FLAGS);
    // normally we would have to call create and initialize first, but they are empty for SphericalGravity
    gravity1.finalize(storage);

    Storage expected = Tests::getGassStorage(1000, settings, Constants::au);
    SphericalGravity gravity2(SphericalGravity::Options::ASSUME_HOMOGENEOUS);
    gravity2.finalize(expected);

    ArrayView<const Vector> dv1 = storage.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<const Vector> dv2 = expected.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    /*StdOutLogger().write("Temporarily disabled test");
    if (true) {
        return;
    }*/
    auto test = [&](const Size i) -> Outcome { //
        if (getLength(r[i]) < 0.1_f * Constants::au) {
            return SUCCESS;
        }
        /// \todo fix this huge discrepancy
        return makeOutcome(dv1[i] == approx(dv2[i], 0.2_f), //
            "invalid acceleration:\n",
            dv1[i],
            " == ",
            dv2[i],
            "\n r = ",
            r[i]);
    };
    REQUIRE(dv1.size() > 500); // sanity check
    REQUIRE_SEQUENCE(test, 0, dv1.size());
}
