#include "sph/solvers/AsymmetricSolver.h"
#include "objects/Exceptions.h"
#include "objects/finders/NeighborFinder.h"
#include "quantities/IMaterial.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/Accumulated.h"
#include "sph/equations/HelperTerms.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

void RadiiHashMap::build(ArrayView<const Vector> r, const Float kernelRadius) {
    cellSize = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        cellSize = max(cellSize, r[i][H] * kernelRadius);
    }

    std::unordered_map<Indices, Float, std::hash<Indices>, IndicesEqual> newMap;
    for (Size i = 0; i < r.size(); ++i) {
        // floor needed to properly handle negative values
        const Indices idxs = floor(r[i] / cellSize);
        Float& radius = newMap[idxs];
        radius = max(radius, r[i][H] * kernelRadius);
    }

    // create map by dilating newMap
    map.clear();
    for (const auto& p : newMap) {
        const Indices& idxs0 = p.first;
        Float radius = p.second;
        for (int i = -1; i <= 1; ++i) {
            for (int j = -1; j <= 1; ++j) {
                for (int k = -1; k <= 1; ++k) {
                    const Indices idxs = idxs0 + Indices(i, j, k);
                    auto iter = newMap.find(idxs);
                    if (iter != newMap.end()) {
                        radius = max(radius, iter->second);
                    }
                }
            }
        }
        map[idxs0] = radius;
    }
}

Float RadiiHashMap::getRadius(const Vector& r) const {
    const Indices idxs = floor(r / cellSize);
    Float radius = 0._f;
    const auto iter = map.find(idxs);
    if (iter != map.end()) {
        radius = max(radius, iter->second);
    }
    return radius;
}

IAsymmetricSolver::IAsymmetricSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& eqs)
    : scheduler(scheduler) {
    kernel = Factory::getKernel<DIMENSIONS>(settings);
    finder = Factory::getFinder(settings);
    equations += eqs;

    if (settings.get<bool>(RunSettingsId::SPH_ASYMMETRIC_COMPUTE_RADII_HASH_MAP)) {
        radiiMap.emplace();
    }
}

void IAsymmetricSolver::integrate(Storage& storage, Statistics& stats) {
    VERBOSE_LOG

    // initialize all materials (compute pressure, apply yielding and damage, ...)
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        PROFILE_SCOPE("IAsymmetricSolver initialize materials")
        MaterialView material = storage.getMaterial(i);
        material->initialize(scheduler, storage, material.sequence());
    }

    // initialize equations, derivatives, accumulate storages, ...
    this->beforeLoop(storage, stats);

    // main loop over pairs of interacting particles
    this->loop(storage, stats);

    // store results to storage, finalizes equations, save statistics, ...
    this->afterLoop(storage, stats);

    // finalize all materials (integrate fragmentation model)
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        PROFILE_SCOPE("IAsymmetricSolver finalize materials")
        MaterialView material = storage.getMaterial(i);
        material->finalize(scheduler, storage, material.sequence());
    }
}

void IAsymmetricSolver::create(Storage& storage, IMaterial& material) const {
    storage.insert<Size>(QuantityId::NEIGHBOR_CNT, OrderEnum::ZERO, 0);
    equations.create(storage, material);
    this->sanityCheck(storage);
}

Float IAsymmetricSolver::getMaxSearchRadius(const Storage& storage) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Float maxH = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        maxH = max(maxH, r[i][H]);
    }
    return maxH * kernel.radius();
}

RawPtr<const IBasicFinder> IAsymmetricSolver::getFinder(ArrayView<const Vector> r) {
    VERBOSE_LOG
    finder->build(scheduler, r);
    return &*finder;
}

AsymmetricSolver::AsymmetricSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& eqs)
    : AsymmetricSolver(scheduler, settings, eqs, Factory::getBoundaryConditions(settings)) {}

AsymmetricSolver::AsymmetricSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& eqs,
    AutoPtr<IBoundaryCondition>&& bc)
    : IAsymmetricSolver(scheduler, settings, eqs)
    , bc(std::move(bc))
    , threadData(scheduler) {

    // creates all derivatives required by the equation terms
    equations.setDerivatives(derivatives, settings);
}

AsymmetricSolver::~AsymmetricSolver() = default;


void AsymmetricSolver::beforeLoop(Storage& storage, Statistics& stats) {
    VERBOSE_LOG

    // initialize boundary conditions first, as they may change the number of particles (ghosts, killbox, ...)
    bc->initialize(storage);

    // initialize all equation terms (applies dependencies between quantities)
    const Float t = stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f);
    equations.initialize(scheduler, storage, t);

    // sets up references to storage buffers for all derivatives
    derivatives.initialize(storage);
}

void AsymmetricSolver::loop(Storage& storage, Statistics& UNUSED(stats)) {
    VERBOSE_LOG

    // (re)build neighbor-finding structure; this needs to be done after all equations
    // are initialized in case some of them modify smoothing lengths
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const IBasicFinder& actFinder = *this->getFinder(r);

    // precompute the search radii
    Float maxRadius = 0._f;
    if (radiiMap) {
        radiiMap->build(r, kernel.radius());
    } else {
        maxRadius = this->getMaxSearchRadius(storage);
    }

    ArrayView<Size> neighs = storage.getValue<Size>(QuantityId::NEIGHBOR_CNT);

    // we need to symmetrize kernel in smoothing lenghts to conserve momentum
    SymmetrizeSmoothingLengths<const LutKernel<DIMENSIONS>&> symmetrizedKernel(kernel);

    auto functor = [this, r, &neighs, maxRadius, &symmetrizedKernel, &actFinder](Size i, ThreadData& data) {
        // max possible radius of r[j]
        const Float neighborRadius = radiiMap ? radiiMap->getRadius(r[i]) : maxRadius;
        SPH_ASSERT(neighborRadius > 0._f);

        // max possible value of kernel.radius() * hbar
        const Float radius = 0.5_f * (r[i][H] * kernel.radius() + neighborRadius);

        actFinder.findAll(i, radius, data.neighs);
        data.grads.clear();
        data.idxs.clear();
        for (auto& n : data.neighs) {
            const Size j = n.index;
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            SPH_ASSERT(hbar > EPS, hbar);
            if (i == j || n.distanceSqr >= sqr(kernel.radius() * hbar)) {
                // aren't actual neighbors
                continue;
            }
            const Vector gr = symmetrizedKernel.grad(r[i], r[j]);
            SPH_ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) <= 0._f, gr, r[i] - r[j]);
            data.grads.emplaceBack(gr);
            data.idxs.emplaceBack(j);
        }
        derivatives.eval(i, data.idxs, data.grads);
        neighs[i] = data.idxs.size();
    };
    PROFILE_SCOPE("AsymmetricSolver::loop");
    parallelFor(scheduler, threadData, 0, r.size(), functor);
}

void AsymmetricSolver::afterLoop(Storage& storage, Statistics& stats) {
    VERBOSE_LOG

    // store the computed values into the storage
    Accumulated& accumulated = derivatives.getAccumulated();
    accumulated.store(storage);

    // using the stored values, integrates all equation terms
    const Float t = stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f);
    equations.finalize(scheduler, storage, t);

    // lastly, finalize boundary conditions, to make sure the computed quantities will not change any further
    bc->finalize(storage);

    // compute neighbor statistics
    ArrayView<Size> neighs = storage.getValue<Size>(QuantityId::NEIGHBOR_CNT);
    MinMaxMean neighsStats;
    const Size size = storage.getParticleCnt();
    for (Size i = 0; i < size; ++i) {
        neighsStats.accumulate(neighs[i]);
    }
    stats.set(StatisticsId::NEIGHBOR_COUNT, neighsStats);
}

void AsymmetricSolver::sanityCheck(const Storage& UNUSED(storage)) const {
    // we must solve smoothing length somehow
    if (!equations.contains<AdaptiveSmoothingLength>() && !equations.contains<ConstSmoothingLength>()) {
        throw InvalidSetup(
            "No solver of smoothing length specified; add either ConstSmootingLength or "
            "AdaptiveSmootingLength into the list of equations");
    }

    // we allow both velocity divergence and density velocity divergence as the former can be used by some
    // terms (e.g. Balsara switch) even in Standard formulation
}

NAMESPACE_SPH_END
