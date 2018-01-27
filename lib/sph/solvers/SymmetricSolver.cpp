#include "sph/solvers/SymmetricSolver.h"
#include "sph/boundary/Boundary.h"
//#include "sph/equations/Accumulated.h"
#include "sph/equations/HelperTerms.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

SymmetricSolver::SymmetricSolver(const RunSettings& settings, const EquationHolder& eqs)
    : pool(makeShared<ThreadPool>(settings.get<int>(RunSettingsId::RUN_THREAD_CNT)))
    , threadData(*pool) {
    /// \todo we have to somehow enforce either conservation of smoothing length or some EquationTerm that
    /// will evolve it. Or maybe just move smoothing length to separate quantity to get rid of these
    /// issues?
    kernel = Factory::getKernel<DIMENSIONS>(settings);
    finder = Factory::getFinder(settings);
    bc = Factory::getBoundaryConditions(settings);

    granularity = settings.get<int>(RunSettingsId::RUN_THREAD_GRANULARITY);
    equations += eqs;

    // add term counting number of neighbours
    equations += makeTerm<NeighbourCountTerm>();

    // initialize all derivatives
    threadData.forEach([this, &settings](ThreadData& data) { //
        equations.setDerivatives(data.derivatives, settings);

        // all derivatives must be symmetric!
        if (!data.derivatives.isSymmetric()) {
            throw InvalidSetup("Asymmetric derivative used within symmetric solver");
        }
    });
}

SymmetricSolver::~SymmetricSolver() = default;

void SymmetricSolver::integrate(Storage& storage, Statistics& stats) {
    // initialize all materials (compute pressure, apply yielding and damage, ...)
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        PROFILE_SCOPE("SymmetricSolver initialize materials")
        MaterialView material = storage.getMaterial(i);
        material->initialize(storage, material.sequence());
    }

    // initialize all equation terms (applies dependencies between quantities)
    equations.initialize(storage);

    // apply boundary conditions before the loop
    bc->initialize(storage);

    // initialize accumulate storages & derivatives
    this->beforeLoop(storage, stats);

    // main loop over pairs of interacting particles
    this->loop(storage, stats);

    // sum up accumulated storage, compute statistics
    this->afterLoop(storage, stats);

    // integrate all equations
    equations.finalize(storage);

    // apply boundary conditions after the loop
    bc->finalize(storage);

    // finalize all materials (integrate fragmentation model)
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        PROFILE_SCOPE("SymmetricSolver finalize materials")
        MaterialView material = storage.getMaterial(i);
        material->finalize(storage, material.sequence());
    }
}

void SymmetricSolver::create(Storage& storage, IMaterial& material) const {
    storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);

    // create necessary quantities
    equations.create(storage, material);

    // check equations and create quantities
    this->sanityCheck(storage);
}

void SymmetricSolver::loop(Storage& storage, Statistics& UNUSED(stats)) {
    // (re)build neighbour-finding structure; this needs to be done after all equations
    // are initialized in case some of them modify smoothing lengths
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder->build(r);

    auto functor = [this, r](const Size n1, const Size n2, ThreadData& data) {
        for (Size i = n1; i < n2; ++i) {
            // Find all neighbours within kernel support. Since we are only searching for particles with
            // smaller h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal
            // to h_i, and we thus never "miss" a particle.
            const FinderFlag flags = FinderFlag::FIND_ONLY_SMALLER_H;
            finder->findNeighbours(i, r[i][H] * kernel.radius(), data.neighs, flags);
            data.grads.clear();
            data.idxs.clear();
            for (auto& n : data.neighs) {
                const Size j = n.index;
                const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                ASSERT(hbar > EPS && hbar <= r[i][H], hbar, r[i][H]);
                if (getSqrLength(r[i] - r[j]) >= sqr(this->kernel.radius() * hbar)) {
                    // aren't actual neighbours
                    continue;
                }
                const Vector gr = kernel.grad(r[i], r[j]);
                ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) <= 0._f, gr, getLength(r[i] - r[j]));
                data.grads.emplaceBack(gr);
                data.idxs.emplaceBack(j);
            }
            data.derivatives.evalSymmetric(i, data.idxs, data.grads);
        }
    };
    PROFILE_SCOPE("GenericSolver main loop");
    parallelFor(*pool, threadData, 0, r.size(), granularity, functor);
}

void SymmetricSolver::beforeLoop(Storage& storage, Statistics& UNUSED(stats)) {
    // clear thread local storages
    PROFILE_SCOPE("GenericSolver::beforeLoop");
    threadData.forEach([&storage](ThreadData& data) { data.derivatives.initialize(storage); });
}

void SymmetricSolver::afterLoop(Storage& storage, Statistics& stats) {
    Accumulated* first = nullptr;
    {
        // sum up thread local accumulated values
        Array<Accumulated*> threadLocalAccumulated;
        threadData.forEach([&first, &threadLocalAccumulated](ThreadData& data) {
            if (!first) {
                first = &data.derivatives.getAccumulated();
            } else {
                ASSERT(first != nullptr);
                threadLocalAccumulated.push(&data.derivatives.getAccumulated());
            }
        });
        ASSERT(first != nullptr);
        PROFILE_SCOPE("GenericSolver::afterLoop part 1");
        first->sum(*pool, threadLocalAccumulated);
    }
    PROFILE_SCOPE("GenericSolver::afterLoop part 2");
    ASSERT(first);
    // store them to storage
    first->store(storage);

    // compute neighbour statistics
    MinMaxMean neighs;
    ArrayView<Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
    const Size size = storage.getParticleCnt();
    for (Size i = 0; i < size; ++i) {
        neighs.accumulate(neighCnts[i]);
    }
    stats.set(StatisticsId::NEIGHBOUR_COUNT, neighs);
}

void SymmetricSolver::sanityCheck(const Storage& storage) const {
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