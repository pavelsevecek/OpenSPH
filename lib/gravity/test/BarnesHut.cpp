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
    return Tests::getGassStorage(1000, settings, r0);
}

static Tuple<Array<Vector>, Array<Float>, Vector> getTestParticles() {
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

    return { std::move(r), std::move(m), r_com };
}

static void testOpeningAngle(const MultipoleOrder order) {
    Storage storage = getGravityStorage();

    // with theta = 0, the BarnetHut should be identical to brute force summing
    BarnesHut bh(0._f, order);
    BruteForceGravity bf;

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    bf.build(r, m);
    bh.build(r, m);

    auto test = [&](const Size i) -> Outcome {
        const Vector a_bf = bf.eval(i);
        const Vector a_bh = bh.eval(i);
        if (a_bf != approx(a_bh)) {
            return makeFailed("Incorrect acceleration: ", a_bh, " == ", a_bf);
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

TEST_CASE("BarnesHut empty", "[gravity]") {
    // no particles = no acceleration
    BarnesHut bh(0.5_f, MultipoleOrder::OCTUPOLE);

    Array<Vector> r;
    Array<Float> m;
    REQUIRE_NOTHROW(bh.build(r, m));

    REQUIRE(bh.eval(Vector(0._f)) == Vector(0._f));
}

static void testMoments(const MultipoleExpansion<3>& moments,
    ArrayView<const Vector> r,
    ArrayView<const Float> m,
    const Vector& r_com) {

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
    Array<Vector> r;
    Array<Float> m;
    Vector r_com;
    tieToTuple(r, m, r_com) = getTestParticles();
    BarnesHut bh(0.5_f, MultipoleOrder::OCTUPOLE);
    bh.build(r, m);

    MultipoleExpansion<3> moments = bh.getMoments();
    testMoments(moments, r, m, r_com);
}

TEST_CASE("BarnesHut storage moments", "[gravity]") {
    // test that the moments in root node correspond to the moments computed from all particles
    Storage storage = getGravityStorage();

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    BarnesHut bh(0.5_f, MultipoleOrder::OCTUPOLE);
    bh.build(r, m);

    MultipoleExpansion<3> moments = bh.getMoments();

    const Vector r_com = CenterOfMass().evaluate(storage);
    testMoments(moments, r, m, r_com);
}

static void testSimpleAcceleration(const MultipoleOrder order, const Float eps) {
    Array<Vector> r;
    Array<Float> m;
    Vector r_com;
    tieToTuple(r, m, r_com) = getTestParticles();

    BarnesHut bh(0.5_f, order, 1);
    bh.build(r, m);

    Vector a = bh.eval(Vector(-10, 10, 0)) / Constants::gravity;
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
    Storage storage = getGravityStorage();

    BarnesHut bh(0.4_f, order);
    BruteForceGravity bf;

    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        // make ellipsoid
        const Float h = r[i][H];
        r[i] *= Vector(2._f, 0.5_f, 0.1_f);
        r[i][H] = h;
    }

    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    bf.build(r, m);
    bh.build(r, m);

    auto test = [&](const Size i) -> Outcome {
        const Vector a_bf = bf.eval(i);
        const Vector a_bh = bh.eval(i);
        if (a_bf == a_bh) {
            return makeFailed("Approximative solution is EXACTLY equal to brute force: ", a_bh, " == ", a_bf);
        }
        if (a_bf != approx(a_bh, eps)) {
            return makeFailed("Incorrect acceleration: ",
                a_bh,
                " == ",
                a_bf,
                "\n eps = ",
                eps,
                "\n difference == ",
                getLength(a_bh - a_bf));
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
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    bf.build(r, m);
    bh2.build(r, m);
    bh4.build(r, m);
    bh8.build(r, m);

    auto test = [&](const Size i) -> Outcome {
        const Vector a_bf = bf.eval(i);
        const Vector a_bh2 = bh2.eval(i);
        const Vector a_bh4 = bh4.eval(i);
        const Vector a_bh8 = bh8.eval(i);

        const Float diff2 = getLength(a_bh2 - a_bf);
        const Float diff4 = getLength(a_bh4 - a_bf);
        const Float diff8 = getLength(a_bh8 - a_bf);

        if (diff2 > diff4 || diff4 > diff8) {
            return makeFailed("Bigger error with smaller opening angle: \n Brute force = ",
                a_bf,
                "\n Theta = 0.2, 0.4, 0.8: ",
                diff2,
                ", ",
                diff4,
                ", ",
                diff8);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}
