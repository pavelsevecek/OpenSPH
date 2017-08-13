#include "sph/equations/Potentials.h"
#include "catch.hpp"
#include "tests/Setup.h"
#include "tests/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("SphericalGravity analytic", "[equationterm]") {
    const Float r0 = 2._f;
    const Float rho0 = 5._f;
    Analytic::StaticSphere sphere(r0, rho0);
    Vector a;
    // linear dependence inside the sphere
    Vector r(0.5_f, 0._f, 0._f);
    a = sphere.getAcceleration(r) / Constants::gravity;
    REQUIRE(a == approx(-rho0 * sphereVolume(1._f) * r));
    r = Vector(1.2_f, 0._f, 0._f);
    a = sphere.getAcceleration(r) / Constants::gravity;
    REQUIRE(a == approx(-rho0 * sphereVolume(1._f) * r));

    // inverse square law outside
    r = Vector(3._f, 1._f, 0._f);
    a = sphere.getAcceleration(r) / Constants::gravity;
    REQUIRE(a == approx(-rho0 * sphereVolume(r0) * r / pow<3>(getLength(r))));
}

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
