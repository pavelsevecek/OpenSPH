#include "sph/solvers/AsymmetricSolver.h"
#include "sph/equations/Accumulated.h"
#include "sph/equations/EquationTerm.h"
#include "sph/equations/HelperTerms.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

AsymmetricSolver::AsymmetricSolver(const RunSettings& settings, const EquationHolder& eqs)
    : pool(makeShared<ThreadPool>(settings.get<int>(RunSettingsId::RUN_THREAD_CNT)))
    , threadData(*pool) {
    kernel = Factory::getKernel<DIMENSIONS>(settings);
    finder = Factory::getFinder(settings);
    granularity = settings.get<int>(RunSettingsId::RUN_THREAD_GRANULARITY);
    equations += eqs;

    // initialize all derivatives
    equations.setDerivatives(derivatives, settings);
}

void AsymmetricSolver::integrate(Storage& storage, Statistics& stats) {
    // initialize all materials (compute pressure, apply yielding and damage, ...)
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        PROFILE_SCOPE("AsymmetricSolver initialize materials")
        MaterialView material = storage.getMaterial(i);
        material->initialize(storage, material.sequence());
    }

    // initialize all equation terms (applies dependencies between quantities)
    equations.initialize(storage);

    // initialize accumulate storages & derivatives
    derivatives.initialize(storage);

    // main loop over pairs of interacting particles
    this->loop(storage, stats);

    // integrate all equations
    equations.finalize(storage);

    // finalize all materials (integrate fragmentation model)
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        PROFILE_SCOPE("AsymmetricSolver finalize materials")
        MaterialView material = storage.getMaterial(i);
        material->finalize(storage, material.sequence());
    }
}

void AsymmetricSolver::create(Storage& storage, IMaterial& material) const {
    storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
    equations.create(storage, material);
    this->sanityCheck(storage);
}

void AsymmetricSolver::loop(Storage& storage, Statistics& stats) {
    // (re)build neighbour-finding structure; this needs to be done after all equations
    // are initialized in case some of them modify smoothing lengths
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder->build(r);

    // find the maximum search radius
    Float maxH = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        maxH = max(maxH, r[i][H]);
    }
    const Float radius = maxH * kernel.radius();

    ArrayView<Size> neighs = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);

    auto functor = [this, r, &neighs, radius](const Size n1, const Size n2, ThreadData& data) {
        for (Size i = n1; i < n2; ++i) {
            finder->findNeighbours(i, radius, data.neighs, EMPTY_FLAGS);
            data.grads.clear();
            data.idxs.clear();
            for (auto& n : data.neighs) {
                const Size j = n.index;
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                ASSERT(hbar > EPS, hbar);
                if (i == j || getSqrLength(r[i] - r[j]) >= sqr(this->kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                const Vector gr = kernel.grad(r[i], r[j]);
                ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) < 0._f, gr, r[i] - r[j]);
                data.grads.emplaceBack(gr);
                data.idxs.emplaceBack(j);
            }
            derivatives.eval(i, data.idxs, data.grads);
            neighs[i] = data.idxs.size();
        }
    };
    PROFILE_SCOPE("AsymmetricSolver main loop");
    parallelFor(*pool, threadData, 0, r.size(), granularity, functor);

    // store accumulated to storage
    Accumulated& accumulated = derivatives.getAccumulated();
    accumulated.store(storage);

    // compute neighbour statistics
    MinMaxMean neighsStats;
    const Size size = storage.getParticleCnt();
    for (Size i = 0; i < size; ++i) {
        neighsStats.accumulate(neighs[i]);
    }
    stats.set(StatisticsId::NEIGHBOUR_COUNT, neighsStats);
}

void AsymmetricSolver::sanityCheck(const Storage& storage) const {
    /// \todo deduplicate ?

    // we must solve smoothing length somehow
    if (!equations.contains<StandardSph::AdaptiveSmoothingLength>() &&
        !equations.contains<BenzAsphaugSph::AdaptiveSmoothingLength>() &&
        !equations.contains<ConstSmoothingLength>()) {
        throw InvalidSetup(
            "No solver of smoothing length specified; add either ConstSmootingLength or "
            "AdaptiveSmootingLength into the list of equations");
    }

    // check for incompatible quantities
    if (storage.has(QuantityId::STRENGTH_VELOCITY_GRADIENT) &&
        storage.has(QuantityId::STRENGTH_DENSITY_VELOCITY_GRADIENT)) {
        throw InvalidSetup(
            "Storage contains both strength velocity gradient and density strength velocity gradient, this "
            "probably means that equations from different SPH formulations are used together.");
    }

    // we allow both velocity divergence and density velocity divergence as the former can be used by some
    // terms (e.g. Balsara switch) even in Standard formulation
}

NAMESPACE_SPH_END