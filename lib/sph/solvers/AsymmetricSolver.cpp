#include "sph/solvers/AsymmetricSolver.h"
#include "objects/Exceptions.h"
#include "objects/finders/NeighbourFinder.h"
#include "quantities/IMaterial.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/Accumulated.h"
#include "sph/equations/HelperTerms.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

IAsymmetricSolver::IAsymmetricSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& eqs)
    : scheduler(scheduler) {
    kernel = Factory::getKernel<DIMENSIONS>(settings);
    finder = Factory::getFinder(settings);
    granularity = settings.get<int>(RunSettingsId::RUN_THREAD_GRANULARITY);
    equations += eqs;
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
    storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
    equations.create(storage, material);
    this->sanityCheck(storage);
}

Float IAsymmetricSolver::getSearchRadius(const Storage& storage) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Float maxH = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        maxH = max(maxH, r[i][H]);
    }
    return maxH * kernel.radius();
}

const IBasicFinder& IAsymmetricSolver::getFinder(ArrayView<const Vector> r) {
    VERBOSE_LOG
    finder->build(scheduler, r);
    return *finder;
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


void AsymmetricSolver::beforeLoop(Storage& storage, Statistics& UNUSED(stats)) {
    VERBOSE_LOG

    // initialize boundary conditions first, as they may change the number of particles (ghosts)
    bc->initialize(storage);

    // initialize all equation terms (applies dependencies between quantities)
    equations.initialize(scheduler, storage);

    // sets up references to storage buffers for all derivatives
    derivatives.initialize(storage);
}

void AsymmetricSolver::loop(Storage& storage, Statistics& UNUSED(stats)) {
    VERBOSE_LOG

    // (re)build neighbour-finding structure; this needs to be done after all equations
    // are initialized in case some of them modify smoothing lengths
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    const IBasicFinder& actFinder = this->getFinder(r);

    // find the maximum search radius
    const Float radius = this->getSearchRadius(storage);

    ArrayView<Size> neighs = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);

    // we need to symmetrize kernel in smoothing lenghts to conserve momentum
    SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>> symmetrizedKernel(kernel);

    auto functor = [this, r, &neighs, radius, &symmetrizedKernel, &actFinder](Size i, ThreadData& data) {
        actFinder.findAll(i, radius, data.neighs);
        data.grads.clear();
        data.idxs.clear();
        for (auto& n : data.neighs) {
            const Size j = n.index;
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            ASSERT(hbar > EPS, hbar);
            if (i == j || n.distanceSqr >= sqr(kernel.radius() * hbar)) {
                // aren't actual neighbours
                continue;
            }
            const Vector gr = symmetrizedKernel.grad(r[i], r[j]);
            ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) <= 0._f, gr, r[i] - r[j]);
            data.grads.emplaceBack(gr);
            data.idxs.emplaceBack(j);
        }
        derivatives.eval(i, data.idxs, data.grads);
        neighs[i] = data.idxs.size();
    };
    PROFILE_SCOPE("AsymmetricSolver::loop");
    parallelFor(scheduler, threadData, 0, r.size(), granularity, functor);
}

void AsymmetricSolver::afterLoop(Storage& storage, Statistics& stats) {
    VERBOSE_LOG

    // store the computed values into the storage
    Accumulated& accumulated = derivatives.getAccumulated();
    accumulated.store(storage);

    // using the stored values, integrates all equation terms
    equations.finalize(scheduler, storage);

    // lastly, finalize boundary conditions, to make sure the computed quantities will not change any further
    bc->finalize(storage);

    // compute neighbour statistics
    ArrayView<Size> neighs = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    MinMaxMean neighsStats;
    const Size size = storage.getParticleCnt();
    for (Size i = 0; i < size; ++i) {
        neighsStats.accumulate(neighs[i]);
    }
    stats.set(StatisticsId::NEIGHBOUR_COUNT, neighsStats);
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
