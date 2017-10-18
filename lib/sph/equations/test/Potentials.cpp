#include "sph/equations/Potentials.h"
#include "catch.hpp"
#include "tests/Approx.h"
#include "tests/Setup.h"
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

TEST_CASE("Inertial Centrifugal", "[equationterm]") {
    const Float omega = 1.5_f;
    InertialForce force(Vector(0._f, 0._f, omega));
    Storage storage = Tests::getGassStorage(1000);
    force.finalize(storage);

    ArrayView<const Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);

    auto test = [&](const Size i) -> Outcome {
        const Float r_perp = sqrt(sqr(r[i][X]) + sqr(r[i][Y]));
        const Float centrifugalForce = sqr(omega) * r_perp;
        if (centrifugalForce != approx(getLength(dv[i]))) {
            return makeFailed("invalid acceleration magnitude:\n", centrifugalForce, dv[i]);
        }
        if (dot(r[i], dv[i]) < 0) {
            return makeFailed("invalid acceleration direction:\n", r[i], dv[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("Inertial Coriolis", "[equationterm]") {
    const Float omega = 1.5_f;
    const Float v0 = -5.e10_f; // large value to make centrifugal force negligible
    InertialForce force(Vector(0._f, 0._f, omega));
    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults(), EPS);
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < v.size(); ++i) {
        v[i] = Vector(v0, 0._f, 0._f);
    }
    force.finalize(storage);

    auto test = [&](const Size i) -> Outcome {
        const Float coriolisForce = 2._f * omega * v0;
        if (abs(coriolisForce) != approx(getLength(dv[i]))) {
            return makeFailed("invalid acceleration magnitude:\n", coriolisForce, dv[i]);
        }
        if (dv[i][Y] < 0._f) {
            // (omega x v0)_Y has a positive sign, v0 is negative and there is a negative sign in Coriolis
            // force, so in total the force have a positive component Y
            return makeFailed("invalid acceleration direction:\n", r[i], dv[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}
