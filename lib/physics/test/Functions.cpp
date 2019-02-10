#include "physics/Functions.h"
#include "catch.hpp"
#include "math/Functional.h"
#include "tests/Approx.h"
#include <map>

using namespace Sph;

TEST_CASE("Effective area", "[functions]") {
    REQUIRE(getEffectiveImpactArea(1._f, 0.2_f, 0._f) == 1._f);
    REQUIRE(getEffectiveImpactArea(1._f, 0.2_f, 40 * DEG_TO_RAD) == 1._f);
    REQUIRE(getEffectiveImpactArea(1._f, 0.2_f, (90._f - 1.e-6_f) * DEG_TO_RAD) == approx(0._f, 1.e-6_f));

    const Float a1 = getEffectiveImpactArea(1._f, 0.2_f, 70._f * DEG_TO_RAD);
    REQUIRE(a1 > 0.1_f);
    REQUIRE(a1 < 0.9_f);
    const Float a2 = getEffectiveImpactArea(5._f, 1._f, 70._f * DEG_TO_RAD);
    REQUIRE(a1 == approx(a2));

    REQUIRE(a1 < getEffectiveImpactArea(1._f, 0.15_f, 70._f * DEG_TO_RAD));

    REQUIRE(isContinuous(Interval(0._f, PI / 2._f), 0.002_f, 0.01_f, [](const Float phi) {
        return getEffectiveImpactArea(2._f, 0.4_f, phi);
    }));
}

static std::map<Float, Float> TABULATED_RADII = {
    { 1._f, 425._f },
    { 0.02_f, 115._f },
    { 50._f, 1566._f },
};

static void testImpactorRadius(const Float QoverQ_D) {
    const Float R_pb = 5.e3_f;
    const Float v_imp = 5.e3_f;
    const Float rho = 2700._f;

    const Float r1 = getImpactorRadius(R_pb, v_imp, QoverQ_D, rho);
    REQUIRE(r1 == approx(TABULATED_RADII[QoverQ_D], 0.1_f));

    // check that the impact energy from this impactor is the expected value
    const Float Q = 0.5_f * pow<3>(r1) * sqr(v_imp) / pow<3>(R_pb);
    REQUIRE(Q == approx(QoverQ_D * evalBenzAsphaugScalingLaw(2._f * R_pb, rho)));

    const Float r2 = getImpactorRadius(R_pb, v_imp, 20._f * DEG_TO_RAD, QoverQ_D, rho);
    REQUIRE(r1 == r2); // effective energy but at low impact angles - should be equal to the regular energy

    // Test impactor radius even close to extreme angles.
    // This is currently WRONG! We compute the effective energy from the AREA of impact, so we can deliver
    // UNLIMITED energy into the target if we choose LARGE ENOUGH impactor (as the impact energy scales with
    // r^3). Logically, there is an upper limit to the kinetic energy at extreme impact angles (provided the
    // impact speed is constant), as there is only so much matter we can 'slice off' the target; further
    // increasing the projectile radius does not change anything, we only miss the target with larget
    // impactor. The effective energy should scale with VOLUME, not with AREA!
    // For now, we keep it this way to be at least consistent with the previous work.
    for (Float phi = 80 * DEG_TO_RAD; phi <= 89 * DEG_TO_RAD; phi += 2._f * DEG_TO_RAD) {
        const Float r4 = getImpactorRadius(R_pb, v_imp, phi, QoverQ_D, rho);
        REQUIRE(r4 > r1 + EPS);
        const Float Q_eff =
            0.5_f * pow<3>(r4) * sqr(v_imp) / pow<3>(R_pb) * getEffectiveImpactArea(R_pb, r4, phi);
        REQUIRE(Q_eff == approx(QoverQ_D * evalBenzAsphaugScalingLaw(2._f * R_pb, rho), 1.e-3_f));
    }
}


TEST_CASE("Impactor radius", "[functions]") {
    testImpactorRadius(1._f);
    testImpactorRadius(0.02_f);
    testImpactorRadius(50._f);
}
