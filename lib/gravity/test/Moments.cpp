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
    REQUIRE_NOTHROW(computeReducedMultipole(m4));
    Multipole<3> m3(4._f);
    REQUIRE_NOTHROW(computeReducedMultipole(m3));

    Multipole<2> m2(6._f);
    const TracelessMultipole<2> f2 = computeReducedMultipole(m2);
    // trace subtracted
    REQUIRE((f2.value<0, 0>()) == 0._f);
    REQUIRE((f2.value<1, 1>()) == 0._f);
    REQUIRE((f2.value<2, 2>()) == 0._f); // approx(0._f));
    // off-diagonal elements unchanged
    REQUIRE((f2.value<0, 1>()) == 6._f);
    REQUIRE((f2.value<0, 2>()) == 6._f);
    REQUIRE((f2.value<1, 2>()) == 6._f);

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
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 1._f);
    Storage storage = Tests::getGassStorage(10, settings);
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
    IndexSequence seq(0, r.size());
    REQUIRE(computeMultipole<0>(r, m, r_com, seq).value() == m_total);
    REQUIRE(computeMultipole<0>(r, m, Vector(-2._f), seq).value() == m_total);

    // first moment = dipole, should be zero if computed with center in r_com
    REQUIRE(computeMultipole<1>(r, m, r_com, seq) == approx(Multipole<1>(0._f)));
    // nonzero around other point
    const Vector r0 = Vector(2._f);
    Multipole<1> m1 = computeMultipole<1>(r, m, r0, seq);
    REQUIRE(Vector(m1[0], m1[1], m1[2]) == approx(m_total * (-r0)));

    // second moment, should be generally nonzero
    Multipole<2> m2 = computeMultipole<2>(r, m, r_com, seq);
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

TEST_CASE("Parallel axis theorem", "[gravity]") {
    // check that PAT really gives correct moments
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, 1._f);
    Storage storage = Tests::getGassStorage(10, settings);
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Float> m = storage.getValue<Float>(QuantityId::MASSES);

    IndexSequence seq(0, r.size());
    const Float m0 = computeMultipole<0>(r, m, Vector(0._f), seq).value();
    Multipole<1> m1 = computeMultipole<1>(r, m, Vector(0._f), seq);
    Multipole<2> m2 = computeMultipole<2>(r, m, Vector(0._f), seq);
    Multipole<3> m3 = computeMultipole<3>(r, m, Vector(0._f), seq);
    Multipole<4> m4 = computeMultipole<4>(r, m, Vector(0._f), seq);
    TracelessMultipole<1> q1 = computeReducedMultipole(m1);
    TracelessMultipole<2> q2 = computeReducedMultipole(m2);
    TracelessMultipole<3> q3 = computeReducedMultipole(m3);
    TracelessMultipole<4> q4 = computeReducedMultipole(m4);

    const Vector d(2._f, 3._f, -1._f);
    Multipole<1> md1 = computeMultipole<1>(r, m, d, seq);
    Multipole<2> md2 = computeMultipole<2>(r, m, d, seq);
    Multipole<3> md3 = computeMultipole<3>(r, m, d, seq);
    Multipole<4> md4 = computeMultipole<4>(r, m, d, seq);
    TracelessMultipole<1> qd1 = computeReducedMultipole(md1);
    TracelessMultipole<2> qd2 = computeReducedMultipole(md2);
    TracelessMultipole<3> qd3 = computeReducedMultipole(md3);
    TracelessMultipole<4> qd4 = computeReducedMultipole(md4);

    // the parameter is d = r - r_new, so to evaluate in d we need to pass -d
    TracelessMultipole<1> qpat1 = parallelAxisTheorem(q1, m0, -d);
    TracelessMultipole<2> qpat2 = parallelAxisTheorem(q2, m0, -d);
    TracelessMultipole<3> qpat3 = parallelAxisTheorem(q3, q2, m0, -d);
    TracelessMultipole<4> qpat4 = parallelAxisTheorem(q4, q3, q2, m0, -d);

    REQUIRE(qd1 == approx(qpat1));
    REQUIRE(qd2 == approx(qpat2));
    REQUIRE(qd3 == approx(qpat3));

    /// \todo
    MARK_USED(qpat4);
    MARK_USED(qd4);
    // REQUIRE(qd4 == approx(qpat4));
}
