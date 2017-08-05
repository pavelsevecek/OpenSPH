#include "gravity/BarnesHut.h"
#include "catch.hpp"
#include "gravity/BruteForceGravity.h"
#include "gravity/Moments.h"
#include "physics/Integrals.h"
#include "tests/Setup.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

static Storage getGravityStorage() {
    const Float r0 = 1.e7_f;
    const Float rho0 = 100._f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    return Tests::getGassStorage(30, settings, r0);
}

static Tuple<Storage, Vector> getTestParticles() {
    Array<Vector> r{
        Vector(2, 3, 0, 1),
        Vector(5, 4, 0, 1),
        Vector(9, 6, 0, 1),
        Vector(4, 7, 0, 1),
        Vector(8, 1, 0, 1),
        Vector(7, 2, 0, 1),
    };
    Array<Float> m(r.size());
    m.fill(1._f);

    Vector r_com(0._f);
    for (Vector& v : r) {
        r_com += v;
    }
    r_com /= r.size();

    Storage storage;
    storage.insert(QuantityId::POSITIONS, OrderEnum::SECOND, std::move(r));
    storage.insert(QuantityId::MASSES, OrderEnum::ZERO, std::move(m));
    return { std::move(storage), r_com };
}

static void testOpeningAngle(const MultipoleOrder order) {
    Storage storage1 = getGravityStorage();
    Storage storage2 = storage1.clone(VisitorEnum::ALL_BUFFERS);

    // with theta = 0, the BarnetHut should be identical to brute force summing
    BarnesHut bh(EPS, order);
    BruteForceGravity bf;

    bf.build(storage1);
    bh.build(storage2);

    ArrayView<Vector> a_bf = storage1.getD2t<Vector>(QuantityId::POSITIONS);
    ArrayView<Vector> a_bh = storage2.getD2t<Vector>(QuantityId::POSITIONS);
    Statistics stats;
    bf.evalAll(a_bf, stats);
    bh.evalAll(a_bh, stats);

    ArrayView<const Vector> r = storage1.getValue<Vector>(QuantityId::POSITIONS);
    auto test = [&](const Size i) -> Outcome {
        if (a_bf[i] != approx(a_bh[i])) {
            return makeFailed("Incorrect acceleration: ", a_bh[i], " == ", a_bf[i]);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("BarnesHut zero opening angle", "[gravity]") {
    testOpeningAngle(MultipoleOrder::MONOPOLE);
    testOpeningAngle(MultipoleOrder::QUADRUPOLE);
    testOpeningAngle(MultipoleOrder::OCTUPOLE);
}

/*TEST_CASE("BarnesHut empty", "[gravity]") {
    // no particles = no acceleration
    BarnesHut bh(0.5_f, MultipoleOrder::OCTUPOLE);

    Storage storage;
    REQUIRE_NOTHROW(bh.build(storage));

    Statistics stats;
    REQUIRE(bh.eval(Vector(0._f), stats) == Vector(0._f));
}
*/
static void testMoments(const MultipoleExpansion<3>& moments, const Storage& storage, const Vector& r_com) {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    IndexSequence seq(0, r.size());
    Multipole<0> m0 = computeMultipole<0>(r, m, r_com, seq);
    Multipole<2> m2 = computeMultipole<2>(r, m, r_com, seq);
    Multipole<3> m3 = computeMultipole<3>(r, m, r_com, seq);
    TracelessMultipole<0> q0 = computeReducedMultipole(m0);
    TracelessMultipole<2> q2 = computeReducedMultipole(m2);
    TracelessMultipole<3> q3 = computeReducedMultipole(m3);

    REQUIRE(moments.order<0>() == approx(q0, 1.e-10_f));
    REQUIRE(moments.order<2>() == approx(q2, 1.e-10_f));
    REQUIRE(moments.order<3>() == approx(q3, 1.e-10_f));
}

TEST_CASE("BarnesHut simple moments", "[gravity]") {
    // test that the moments in root node correspond to the moments computed from all particles
    Storage storage;
    Vector r_com;
    tieToTuple(storage, r_com) = getTestParticles();
    BarnesHut bh(0.5_f, MultipoleOrder::OCTUPOLE);
    bh.build(storage);

    MultipoleExpansion<3> moments = bh.getMoments();
    testMoments(moments, storage, r_com);
}

TEST_CASE("BarnesHut storage moments", "[gravity]") {
    // test that the moments in root node correspond to the moments computed from all particles
    Storage storage = getGravityStorage();

    BarnesHut bh(0.5_f, MultipoleOrder::OCTUPOLE);
    bh.build(storage);

    MultipoleExpansion<3> moments = bh.getMoments();

    const Vector r_com = CenterOfMass().evaluate(storage);
    testMoments(moments, storage, r_com);
}

static void testSimpleAcceleration(const MultipoleOrder order, const Float eps) {
    Storage storage;
    Vector r_com;
    tieToTuple(storage, r_com) = getTestParticles();

    BarnesHut bh(0.5_f, order, 1);
    bh.build(storage);

    Statistics stats;
    Vector a = bh.eval(Vector(-10, 10, 0, 1), stats) / Constants::gravity;
    Vector expected(0.020169998934707004, -0.007912678499211458, 0);
    REQUIRE(a != expected); // it shouldn't be exactly equal, sanity check
    REQUIRE(a == approx(expected, eps));
}

TEST_CASE("BarnesHut simple acceleration", "[gravity]") {
    testSimpleAcceleration(MultipoleOrder::MONOPOLE, 4.e-4_f);
    testSimpleAcceleration(MultipoleOrder::QUADRUPOLE, 8.e-5_f);
    testSimpleAcceleration(MultipoleOrder::OCTUPOLE, 1.e-5_f);
}

static void testStorageAcceleration(const MultipoleOrder order, const Float eps) {
    Storage storage1 = getGravityStorage();

    BarnesHut bh(0.4_f, order);
    BruteForceGravity bf;

    ArrayView<Vector> r = storage1.getValue<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        // make ellipsoid
        const Float h = r[i][H];
        r[i] *= Vector(2._f, 0.5_f, 0.1_f);
        r[i][H] = h;
    }
    Storage storage2 = storage1.clone(VisitorEnum::ALL_BUFFERS);

    bf.build(storage1);
    bh.build(storage2);

    ArrayView<Vector> a_bf = storage1.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<Vector> a_bh = storage2.getValue<Vector>(QuantityId::POSITIONS);
    Statistics stats;
    bf.evalAll(a_bf, stats);
    bh.evalAll(a_bh, stats);
    auto test = [&](const Size i) -> Outcome {
        if (a_bf[i] == a_bh[i]) {
            return makeFailed("Approximative solution is EXACTLY equal to brute force: ", a_bh, " == ", a_bf);
        }
        if (a_bf[i] != approx(a_bh[i], eps)) {
            return makeFailed("Incorrect acceleration: ",
                a_bh[i],
                " == ",
                a_bf[i],
                "\n eps = ",
                eps,
                "\n difference == ",
                getLength(a_bh[i] - a_bf[i]));
        }
        return SUCCESS;
    };

    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("BarnesHut storage acceleration", "[gravity]") {
    testStorageAcceleration(MultipoleOrder::MONOPOLE, 1.e-2f);
    testStorageAcceleration(MultipoleOrder::QUADRUPOLE, 1.e-3_f);
    testStorageAcceleration(MultipoleOrder::OCTUPOLE, 1.e-4_f);
}

TEST_CASE("BarnesHut opening angle convergence", "[gravity]") {
    Storage storage = getGravityStorage();

    BarnesHut bh8(0.8_f, MultipoleOrder::OCTUPOLE);
    BarnesHut bh4(0.4_f, MultipoleOrder::OCTUPOLE);
    BarnesHut bh2(0.2_f, MultipoleOrder::OCTUPOLE);
    BruteForceGravity bf;
    bf.build(storage);
    bh2.build(storage);
    bh4.build(storage);
    bh8.build(storage);

    Statistics stats;
    Array<Vector> a_bf = storage.getD2t<Vector>(QuantityId::POSITIONS).clone();
    bf.evalAll(a_bf, stats);
    Array<Vector> a_bh2 = storage.getD2t<Vector>(QuantityId::POSITIONS).clone();
    bf.evalAll(a_bh2, stats);
    Array<Vector> a_bh4 = storage.getD2t<Vector>(QuantityId::POSITIONS).clone();
    bf.evalAll(a_bh4, stats);
    Array<Vector> a_bh8 = storage.getD2t<Vector>(QuantityId::POSITIONS).clone();
    bf.evalAll(a_bh8, stats);

    auto test = [&](const Size i) -> Outcome {
        const Float diff2 = getLength(a_bh2[i] - a_bf[i]);
        const Float diff4 = getLength(a_bh4[i] - a_bf[i]);
        const Float diff8 = getLength(a_bh8[i] - a_bf[i]);

        if (diff2 > diff4 || diff4 > diff8) {
            return makeFailed("Bigger error with smaller opening angle: \n Brute force = ",
                a_bf[i],
                "\n Theta = 0.2, 0.4, 0.8: ",
                diff2,
                ", ",
                diff4,
                ", ",
                diff8);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, a_bf.size());
}
