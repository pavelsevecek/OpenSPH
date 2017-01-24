#include "solvers/Accumulator.h"
#include "catch.hpp"
#include "geometry/Domain.h"
#include "objects/finders/Voxel.h"
#include "quantities/Storage.h"
#include "sph/initial/Distribution.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Logger.h"

using namespace Sph;


template <typename TAccumulator>
void accumulate(Storage& storage, ArrayView<Vector> r, TAccumulator& accumulator) {
    KdTree finder;
    finder.build(r);
    Array<NeighbourRecord> neighs;
    LutKernel<3> kernel = Factory::getKernel<3>(GLOBAL_SETTINGS);

    accumulator.template update(storage);
    for (Size i = 0; i < r.size(); ++i) {
        finder.findNeighbours(i, kernel.radius() * r[i][H], neighs, FinderFlags::FIND_ONLY_SMALLER_H);
        for (auto& n : neighs) {
            Size j = n.index;
            // all particles have same h, so we dont have to symmetrize
            ASSERT(r[i][H] == r[j][H]);
            ASSERT(getLength(r[i] - r[j]) <= kernel.radius() * r[i][H]);
            accumulator.template accumulate(i, j, kernel.grad(r[i]-r[j], r[i][H]));
        }
    }
}


TEST_CASE("Grad v", "[accumulator]") {
    Storage storage;
    HexagonalPacking distribution;
    storage.emplace<Vector, OrderEnum::SECOND_ORDER>(
        QuantityIds::POSITIONS, distribution.generate(1000, SphericalDomain(Vector(0._f), 1._f)));
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityIds::POSITIONS);
    // density = 1, therefore total mass = volume, therefore mass per particle = volume / N
    storage.emplace<Float, OrderEnum::ZERO_ORDER>(QuantityIds::MASSES, sphereVolume(1._f) / r.size());
    storage.emplace<Size, OrderEnum::ZERO_ORDER>(QuantityIds::FLAG, 0);
    RhoGradv rhoGradv;

    // sanity check that const velocity = zero gradient
    for (Size i = 0; i < v.size(); ++i) {
        v[i] = Vector(2._f, 3._f, -1._f);
    }
    accumulate(storage, r, rhoGradv);
    StdOutLogger logger;
    bool allMatching = true;
    REQUIRE(v.size() > 70);
    for (Size i = 0; i < v.size(); ++i) {
        // here we ALWAYS subtract two equal values, so the result should be zero EXACTLY
        if (rhoGradv[i] != Tensor(0._f)) {
            logger.write("Invalid grad v");
            logger.write("r = ", r[i]);
            logger.write("grad v = ", rhoGradv[i]);
            logger.write("expected = ", Tensor::null());
            allMatching = false;
            break;
        }
    }
    REQUIRE(allMatching);

    // some non-trivial velocity field
    for (Size i = 0; i < v.size(); ++i) {
        v[i] = Vector(r[i][0] * sqr(r[i][1]), r[i][0] + 0.5_f * r[i][2], sin(r[i][2]));
    }
    accumulate(storage, r, rhoGradv);
    allMatching = true;
    for (Size i = 0; i < v.size(); ++i) {
        if (getLength(r[i]) > 1._f - 2._f * r[i][H]) {
            continue;
        }
        // gradient of velocity field
        const Float x = r[i][X];
        const Float y = r[i][Y];
        const Float z = r[i][Z];
        Tensor expected(Vector(sqr(y), 0._f, cos(z)), Vector(0.5_f * (1._f + 2._f * x * y), 0._f, 0.25_f));
        if (!almostEqual(rhoGradv[i], expected, 3.e-2_f)) {
            logger.write("Invalid grad v");
            logger.write("r = ", r[i]);
            logger.write("grad v = ", rhoGradv[i]);
            logger.write("expected = ", expected);
            allMatching = false;
            break;
        }
    }
    REQUIRE(allMatching);
}
