#include "sph/solvers/PositionBasedSolver.h"
#include "gravity/IGravity.h"
#include "objects/finders/NeighborFinder.h"
#include "quantities/Quantity.h"
#include "system/Factory.h"
#include "system/Statistics.h"
#include "system/Timer.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

PositionBasedSolver::PositionBasedSolver(IScheduler& scheduler, const RunSettings& settings)
    : scheduler(scheduler) {
    poly6 = Poly6();
    spiky = SpikyKernel();

    finder = Factory::getFinder(settings);
    gravity = Factory::getGravity(settings);

    iterCnt = settings.get<int>(RunSettingsId::PBD_ITERATION_COUNT);
    eps = settings.get<Float>(RunSettingsId::PBD_RELAXATION_PARAMETER);
}

PositionBasedSolver::~PositionBasedSolver() = default;

void PositionBasedSolver::integrate(Storage& storage, Statistics& stats) {
    parallelInvoke(
        scheduler, [&] { this->evalHydro(storage, stats); }, [&] { this->evalGravity(storage, stats); });
}

void PositionBasedSolver::evalHydro(Storage& storage, Statistics& stats) {
    Timer timer;
    ArrayView<Vector> r, v, dv;
    tie(r, v, dv) = storage.getAll<Vector>(QuantityId::POSITION);

    const Float dt = stats.get<Float>(StatisticsId::TIMESTEP_VALUE);

    // predict positions
    Array<Vector> r1(r.size());
    parallelFor(scheduler, 0, r.size(), [&r, &r1, &v, &dv, dt](Size i) { r1[i] = r[i] + v[i] * dt; });

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
    parallelFor(scheduler, 0, r.size(), [this, &neighCnt, &r, &v, &r1, dt](Size i) {
        v[i] = clearH((r1[i] - r[i]) / dt);
        neighCnt[i] = neighbors[i].size();
    });
    stats.set(StatisticsId::SPH_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
}

void PositionBasedSolver::evalGravity(Storage& storage, Statistics& stats) {
    Timer timer;
    ArrayView<Vector> dv = storage.getD2t<Vector>(QuantityId::POSITION);
    gravity->build(scheduler, storage);
    gravity->evalSelfGravity(scheduler, dv, stats);
    stats.set(StatisticsId::GRAVITY_EVAL_TIME, int(timer.elapsed(TimerUnit::MILLISECOND)));
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
    parallelFor(scheduler, 0, r1.size(), [this, &rho1, &r1](Size i) {
        const Float C = rho1[i] / rho0[i] - 1;
        Float sumGradC = 0;
        for (Size j : neighbors[i]) {
            sumGradC += getSqrLength(drho1[j] / rho0[j]);
        }
        lambda[i] = -C / (sumGradC + eps / sqr(r1[i][H]));
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
