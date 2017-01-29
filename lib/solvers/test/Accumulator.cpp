#include "solvers/Accumulator.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/finders/Voxel.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "utils/Approx.h"
#include "utils/SequenceTest.h"

using namespace Sph;

static Storage getStorage() {
    Storage storage;
    HexagonalPacking distribution;
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, distribution.generate(10000, SphericalDomain(Vector(0._f), 1._f)));
    // density = 1, therefore total mass = volume, therefore mass per particle = volume / N
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(
        QuantityIds::MASSES, sphereVolume(1._f) / storage.getParticleCnt());
    REQUIRE(storage.getParticleCnt() > 9000); // sanity check
    return storage;
}

template <typename TAccumulator>
void accumulate(Storage& storage, ArrayView<Vector> r, TAccumulator& accumulator) {
    VoxelFinder finder;
    finder.build(r);
    Array<NeighbourRecord> neighs;
    LutKernel<3> kernel = Factory::getKernel<3>(GlobalSettings::getDefaults());

    accumulator.template update(storage);
    for (Size i = 0; i < r.size(); ++i) {
        finder.findNeighbours(i, kernel.radius() * r[i][H], neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        for (auto& n : neighs) {
            Size j = n.index;
            // all particles have same h, so we dont have to symmetrize
            ASSERT(r[i][H] == r[j][H]);
            ASSERT(getLength(r[i] - r[j]) <= kernel.radius() * r[i][H]);
            accumulator.template accumulate(i, j, kernel.grad(r[i] - r[j], r[i][H]));
        }
    }
}

TEST_CASE("Div v of position vectors", "[accumulator]") {
    Storage storage = getStorage();
    RhoDivv rhoDivv;
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    // check that div r = 3
    for (Size i = 0; i < v.size(); ++i) {
        v[i] = r[i];
    }
    accumulate(storage, r, rhoDivv);

    auto test1 = [&](const Size i) {
        // particles on boundary have different velocity divergence, check only particles inside
        if (getLength(r[i]) > 0.7_f) {
            return SUCCESS;
        }
        if (rhoDivv[i] != approx(3._f, 0.03_f)) {
            return makeFailed("Incorrect velocity divergence: \n",
                              "divv: ", rhoDivv[i], " == -3", "\n particle: r = ", r[i]);
            }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test1, 0, r.size());
}

TEST_CASE("Grad v of const field", "[accumulator]") {
    Storage storage = getStorage();
    storage.emplace<Size, OrderEnum::ZERO_ORDER>(QuantityIds::FLAG, 0);
    RhoGradv rhoGradv;

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    // sanity check that const velocity = zero gradient
    for (Size i = 0; i < v.size(); ++i) {
        v[i] = Vector(2._f, 3._f, -1._f);
    }
    accumulate(storage, r, rhoGradv);

    auto test = [&](const Size i) {
        // here we ALWAYS subtract two equal values, so the result should be zero EXACTLY
        if (rhoGradv[i] != Tensor(0._f)) {
            // clang-format off
            return makeFailed("Invalid grad v"
                              "\n r = ", r[i],
                              "\n grad v = ", rhoGradv[i],
                              "\n expected = ", Tensor::null());
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, v.size());
}

TEST_CASE("Grad v of position vector", "[accumulator]") {
    Storage storage = getStorage();
    storage.emplace<Size, OrderEnum::ZERO_ORDER>(QuantityIds::FLAG, 0);
    RhoGradv rhoGradv;

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    // check that grad r = identity
    for (Size i = 0; i < v.size(); ++i) {
        v[i] = r[i];
    }
    accumulate(storage, r, rhoGradv);

    auto test = [&](const Size i) {
        if (getLength(r[i]) > 0.7_f) {
            return SUCCESS;
        }
        if (rhoGradv[i] != approx(Tensor::identity(), 0.05_f)) {
            // clang-format off
            return makeFailed("Invalid grad v"
                              "\n r = ", r[i],
                              "\n grad v = ", rhoGradv[i],
                              "\n expected = ", Tensor::identity());
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, r.size());
}


TEST_CASE("Grad v of non-trivial field", "[accumulator]") {
    Storage storage = getStorage();
    storage.emplace<Size, OrderEnum::ZERO_ORDER>(QuantityIds::FLAG, 0);
    RhoGradv rhoGradv;

    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    for (Size i = 0; i < v.size(); ++i) {
        v[i] = Vector(r[i][0] * sqr(r[i][1]), r[i][0] + 0.5_f * r[i][2], sin(r[i][2]));
    }
    accumulate(storage, r, rhoGradv);

    auto test = [&](const Size i) {
        if (getLength(r[i]) > 0.7_f) {
            // skip test by reporting success
            return SUCCESS;
        }
        // gradient of velocity field
        const Float x = r[i][X];
        const Float y = r[i][Y];
        const Float z = r[i][Z];
        Tensor expected(Vector(sqr(y), 0._f, cos(z)), Vector(0.5_f * (1._f + 2._f * x * y), 0._f, 0.25_f));
        if (rhoGradv[i] != approx(expected, 0.05_f)) {
            // clang-format off
            return makeFailed("Invalid grad v"
                              "\n r = ", r[i],
                              "\n grad v = ", rhoGradv[i],
                              "\n expected = ", expected);
            // clang-format on
        }
        return SUCCESS;
    };
    REQUIRE_SEQUENCE(test, 0, v.size());
}
