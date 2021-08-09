#include "post/TwoBody.h"
#include "catch.hpp"
#include "objects/utility/Algorithm.h"
#include "physics/Constants.h"
#include "tests/Approx.h"
#include "utils/Utils.h"

using namespace Sph;

TEST_CASE("Keplerian elements", "[kepler]") {
    // test case for Earth
    const Float m = 5.972e24;
    const Float M = 1.989e30;
    const Vector r(0._f, Constants::au, 0._f);
    const Vector v(0._f, 0._f, 29800._f);

    Optional<Kepler::Elements> elements = Kepler::computeOrbitalElements(M + m, m * M / (M + m), r, v);
    REQUIRE(elements);

    REQUIRE(elements->a == approx(Constants::au, 1.e-3f));
    REQUIRE(elements->e == approx(0.0167, 0.1f)); // yup, very uncertain, we just check it's not >1 or whatnot
    REQUIRE(elements->i == approx(PI / 2._f));

    REQUIRE(elements->ascendingNode() == approx(-PI / 2._f));

    // periapsis is too uncertain to actually test anything reasonable
}

TEST_CASE("Eccentric anomaly to true anomaly", "[kepler]") {
    REQUIRE(Kepler::eccentricAnomalyToTrueAnomaly(0._f, 0.2_f) == 0._f);
    REQUIRE(Kepler::eccentricAnomalyToTrueAnomaly(PI, 0.2_f) == PI);
    REQUIRE(Kepler::trueAnomalyToEccentricAnomaly(0._f, 0.2_f) == 0._f);
    REQUIRE(Kepler::trueAnomalyToEccentricAnomaly(PI, 0.2_f) == PI);

    for (Float e : { 0._f, 0.2_f, 0.4_f, 0.7_f, 0.9_f }) {
        Array<Float> expected({ 0._f, 0.5_f, 3._f });
        Array<Float> actual;
        for (Float u : expected) {
            const Float v = Kepler::eccentricAnomalyToTrueAnomaly(u, e);
            actual.push(Kepler::trueAnomalyToEccentricAnomaly(v, e));
        }
        REQUIRE(almostEqual(actual, expected, EPS));
    }
}
