#include "gravity/BarnesHut.h"
#include "catch.hpp"
#include "gravity/BruteForceGravity.h"
#include "physics/Integrals.h"
#include "tests/Setup.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

TEST_CASE("BarnesHut zero opening angle", "[gravity]") {
    // with theta = 0, the BarnetHut should be identical to brute force summing
    const Float r0 = 1.e7_f;
    const Float rho0 = 100._f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getGassStorage(1000, settings, r0);

    BarnesHut bh(0._f);
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

TEST_CASE("BarnesHut empty", "[gravity]") {
    // no particles = no acceleration
    BarnesHut bh(0.5_f);

    Array<Vector> r;
    Array<Float> m;
    REQUIRE_NOTHROW(bh.build(r, m));

    /// \todo this would need overload for vector instead of index
    // REQUIRE(bh.eval(Vector(0._f)) == Vector(0._f));
}

TEST_CASE("BarnesHut simple 2D", "[gravity]") {
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

    BarnesHut bh(0.5_f, 1);
    bh.build(r, m);
    Vector a = bh.eval(Vector(-10, 10, 0)) / Constants::gravity;
    REQUIRE(a == Vector(0.020169998934707004, -0.007912678499211458, 0));
}

TEST_CASE("BarnesHut quadrupole", "[gravity]") {
    const Float r0 = 1.e7_f;
    const Float rho0 = 100._f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    Storage storage = Tests::getGassStorage(1000, settings, r0);

    BarnesHut bh(0.4_f);
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
        if (a_bf != approx(a_bh)) {
            return makeFailed(
                "Incorrect acceleration: ", a_bh, " == ", a_bf, "\n difference == ", getLength(a_bh - a_bf));
        }
        return SUCCESS;
    };

    REQUIRE_SEQUENCE(test, 0, r.size());
}
