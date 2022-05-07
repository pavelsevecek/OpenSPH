#include "sph/solvers/PositionBasedSolver.h"
#include "objects/finders/NeighborFinder.h"
#include "quantities/Quantity.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

PositionBasedSolver::PositionBasedSolver(IScheduler& scheduler, const RunSettings& settings)
    : scheduler(scheduler) {
    poly6 = Poly6();
    spiky = SpikyKernel();

    finder = Factory::getFinder(settings);
    iterCnt = settings.get<int>(RunSettingsId::SPH_POSITION_BASED_ITERATION_COUNT);
}

void PositionBasedSolver::integrate(Storage& storage, Statistics& stats) {
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);
    const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);

    // predict positions
    Array<Vector> r1(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        r1[i] = r[i] + v[i] * dt;
    }

    // find neighbors
    finder->build(scheduler, r1);
    neighbors.resize(r.size());
    ThreadLocal<Array<NeighborRecord>> neighsTl(scheduler);
    parallelFor(scheduler, neighsTl, 0, r.size(), [this, &r1](Size i, Array<NeighborRecord>& neighs) {
        finder->findAll(r1[i], r1[i][H], neighs);
        neighbors[i].resize(neighs.size());
        for (Size j = 0; j < neighs.size(); ++j) {
            neighbors[i][j] = neighs[j].index;
        }
    });

    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    ArrayView<Float> rho1 = storage.getValue<Float>(QuantityId::DENSITY);
    for (Size iter = 0; iter < iterCnt; ++iter) {
        doIteration(r1, rho1, m);
    }

    // velocity update (& other auxiliary quantities)
    ArrayView<Size> neighCnt = storage.getValue<Size>(QuantityId::NEIGHBOR_CNT);
    parallelFor(scheduler, 0, r.size(), [this, &neighCnt, &r, &v, &dv, &r1, dt](Size i) {
        v[i] = clearH((r1[i] - r[i]) / dt);
        neighCnt[i] = neighbors[i].size();
    });
}

void PositionBasedSolver::create(Storage& storage, IMaterial& UNUSED(material)) const {
    storage.insert<Size>(QuantityId::NEIGHBOR_CNT, OrderEnum::ZERO, 0);
}

void PositionBasedSolver::doIteration(Array<Vector>& r1, ArrayView<Float> rho1, ArrayView<const Float> m) {
    drho1.resize(rho1.size());
    parallelFor(scheduler, 0, r1.size(), [this, &r1, &rho1, &m](Size i) {
        rho1[i] = 0;
        drho1[i] = Vector(0._f);
        for (Size j : neighbors[i]) {
            rho1[i] += m[j] * poly6.value(r1[i] - r1[j], r1[j][H]);
            drho1[i] += m[j] * spiky.grad(r1[i] - r1[j], r1[j][H]);
        }
    });
    if (rho0.empty()) {
        // lazy initialization
        rho0.pushAll(rho1.begin(), rho1.end());
    }
    lambda.resize(r1.size());
    parallelFor(scheduler, 0, r1.size(), [this, &rho1](Size i) {
        const Float C = rho1[i] / rho0[i] - 1;
        Float sumGradC = 0;
        for (Size j : neighbors[i]) {
            sumGradC += getSqrLength(drho1[j] / rho0[j]);
        }
        lambda[i] = -C / (sumGradC + eps);
    });
    dp.resize(r1.size());
    parallelFor(scheduler, 0, r1.size(), [this, &r1](Size i) {
        dp[i] = Vector(0._f);
        for (Size j : neighbors[i]) {
            dp[i] += (lambda[i] + lambda[j]) * spiky.grad(r1[i] - r1[j], r1[j][H]);
        }
    });
    parallelFor(scheduler, 0, r1.size(), [this, &r1, &m](Size i) { //
        r1[i] += m[i] / rho0[i] * dp[i];
    });
}

NAMESPACE_SPH_END
