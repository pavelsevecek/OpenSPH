#include "gravity/Moments.h"
#include "catch.hpp"
#include "physics/Integrals.h"
#include "tests/Setup.h"
#include "utils/Approx.h"

using namespace Sph;

TEST_CASE("Moments trace", "[gravity]") {
    Multipole<2> m1(2._f);
    m1.value<1, 1>() = 3._f;

    REQUIRE(computeTrace<1>(m1).value() == 7._f);

    Multipole<3> m2(4._f);
    m2.value<1, 1, 1>() = 1._f;
    m2.value<1, 1, 2>() = 5._f;
    Multipole<1> trM = computeTrace<1>(m2);
    REQUIRE(trM.value<0>() == 12._f);
    REQUIRE(trM.value<1>() == 9._f);
    REQUIRE(trM.value<2>() == 13._f);
}

TEST_CASE("Reduced multipole", "[gravity]") {
    Multipole<4> m4(3._f);

    TracelessMultipole<4> q4 = computeReducedMultipole(m4);
    (void)q4;

    Multipole<1> m1;
    m1.value<0>() = 1._f;
    m1.value<1>() = 3._f;
    m1.value<2>() = 5._f;

    TracelessMultipole<1> q1 = computeReducedMultipole(m1);
    REQUIRE(q1.value<0>() == 1._f);
    REQUIRE(q1.value<1>() == 3._f);
    REQUIRE(q1.value<2>() == 5._f);
}


TEST_CASE("Moments computation", "[gravity]") {
    Storage storage = Tests::getGassStorage(10);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);

    // compute the center of mass
    CenterOfMass com;
    const Vector r_com = com.evaluate(storage);

    // compute the total mass of particles
    Float m_total = 0._f;
    for (Size i = 0; i < m.size(); ++i) {
        m_total += m[i];
    }

    // zero moment = total mass (center doesn't matter)
    REQUIRE(computeMultipole<0>(r, r_com, m).value() == m_total);
    REQUIRE(computeMultipole<0>(r, Vector(-2._f), m).value() == m_total);

    // first moment = dipole, should be zero if computed with center in r_com
    REQUIRE(computeMultipole<1>(r, r_com, m) == approx(Multipole<1>(0._f)));
    // nonzero around other point
    const Vector r0 = Vector(2._f);
    Multipole<1> m1 = computeMultipole<1>(r, r0, m);
    REQUIRE(Vector(m1[0], m1[1], m1[2]) == approx(m_total * (-r0)));

    // second moment, should be generally nonzero
    Multipole<2> m2 = computeMultipole<2>(r, r_com, m);
    REQUIRE(m2 != approx(Multipole<2>(0._f)));
}

/*TEST_CASE("Moments single particle", "[gravity]") {
    // check that the components are as expected
    Array<Vector> r{ Vector(1._f, 2._f, 3._f) };
    Array<Float> m{ 1._f };
    Multipole<2> m2 = computeMultipoleMoments<2>(r, Vector(0._f), m);
    REQUIRE(m(0, 0) == r[0] * r[0]);
    REQUIRE(m(0, 0) == r[0] * r[0]);
}
*/
