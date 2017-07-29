#include "gravity/VoxelGravity.h"
#include "catch.hpp"
#include "gravity/BruteForceGravity.h"
#include "gravity/Moments.h"
#include "physics/Integrals.h"
#include "tests/Setup.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;
/*
static Storage getGravityStorage() {
    const Float r0 = 1.e7_f;
    const Float rho0 = 100._f;
    BodySettings settings;
    settings.set(BodySettingsId::DENSITY, rho0);
    return Tests::getGassStorage(1000, settings, r0);
}

/// \todo heavily duplicated from Barnes-Hut

static void testOpeningAngle(const MultipoleOrder order) {
    Storage storage = getGravityStorage();

    // with theta = 0, the BarnetHut should be identical to brute force summing
    VoxelGravity bh(0._f, order);
    BruteForceGravity bf;

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASSES);
    bf.build(r, m);
    bh.build(r, m);

    auto test = [&](const Size i) -> Outcome {
        Statistics stats;
        const Vector a_bf = bf.eval(i, stats);
        const Vector a_bh = bh.eval(i, stats);
        if (a_bf != approx(a_bh)) {
            return makeFailed("Incorrect acceleration: ", a_bh, " == ", a_bf);
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}

TEST_CASE("Voxel zero opening angle", "[gravity]") {
    testOpeningAngle(MultipoleOrder::MONOPOLE);
    testOpeningAngle(MultipoleOrder::QUADRUPOLE);
    testOpeningAngle(MultipoleOrder::OCTUPOLE);
}

static void testStorageAcceleration(const MultipoleOrder order, const Float eps) {
    Storage storage = getGravityStorage();

    VoxelGravity bh(0.4_f, order);
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

    Statistics stats;
    auto test = [&](const Size i) -> Outcome {
        const Vector a_bf = bf.eval(i, stats);
        const Vector a_bh = bh.eval(i, stats);
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

TEST_CASE("VoxelGravity storage acceleration", "[gravity]") {
    testStorageAcceleration(MultipoleOrder::MONOPOLE, 1.e-2f);
    testStorageAcceleration(MultipoleOrder::QUADRUPOLE, 1.e-3_f);
    /// \todo this has bigger error than Barnes-Hut - coincidence?
    testStorageAcceleration(MultipoleOrder::OCTUPOLE, 2.e-4_f);
}
*/
