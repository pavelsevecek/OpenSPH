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
    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults(), r0, 100._f);

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

TEST_CASE("BarnesHut quadrupole", "[gravity]") {
    const Float r0 = 1.e7_f;
    Storage storage = Tests::getGassStorage(1000, BodySettings::getDefaults(), r0, 100._f);

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
