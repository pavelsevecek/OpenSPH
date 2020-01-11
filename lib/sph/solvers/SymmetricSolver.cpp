#include "sph/solvers/SymmetricSolver.h"
#include "objects/Exceptions.h"
#include "objects/finders/NeighbourFinder.h"
#include "quantities/IMaterial.h"
#include "sph/boundary/Boundary.h"
#include "sph/equations/HelperTerms.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

template <Size Dim>
SymmetricSolver<Dim>::SymmetricSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& eqs,
    AutoPtr<IBoundaryCondition>&& bc)
    : scheduler(scheduler)
    , threadData(scheduler)
    , bc(std::move(bc)) {
    /// \todo we have to somehow enforce either conservation of smoothing length or some EquationTerm that
    /// will evolve it. Or maybe just move smoothing length to separate quantity to get rid of these
    /// issues?
    kernel = Factory::getKernel<Dim>(settings);

    // Find all neighbours within kernel support. Since we are only searching for particles with
    // smaller h, we know that symmetrized lengths (h_i + h_j)/2 will be ALWAYS smaller or equal
    // to h_i, and we thus never "miss" a particle.
    finder = Factory::getFinder(settings);

    equations += eqs;

    // add term counting number of neighbours
    equations += makeTerm<NeighbourCountTerm>();

    // initialize all derivatives
    for (ThreadData& data : threadData) {
        equations.setDerivatives(data.derivatives, settings);

        // all derivatives must be symmetric!
        if (!data.derivatives.isSymmetric()) {
            throw InvalidSetup("Asymmetric derivative used within symmetric solver");
        }
    }
}

template <Size Dim>
SymmetricSolver<Dim>::SymmetricSolver(IScheduler& scheduler,
    const RunSettings& settings,
    const EquationHolder& eqs)
    : SymmetricSolver(scheduler, settings, eqs, Factory::getBoundaryConditions(settings)) {}

template <Size Dim>
SymmetricSolver<Dim>::~SymmetricSolver() = default;

template <Size Dim>
void SymmetricSolver<Dim>::integrate(Storage& storage, Statistics& stats) {
    // initialize all materials (compute pressure, apply yielding and damage, ...)
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        PROFILE_SCOPE("SymmetricSolver initialize materials")
        MaterialView material = storage.getMaterial(i);
        material->initialize(scheduler, storage, material.sequence());
    }

    const Float t = stats.getOr<Float>(StatisticsId::RUN_TIME, 0._f);

    // initialize all equation terms (applies dependencies between quantities)
    equations.initialize(scheduler, storage, t);

    // apply boundary conditions before the loop
    bc->initialize(storage);

    // initialize accumulate storages & derivatives
    this->beforeLoop(storage, stats);

    // main loop over pairs of interacting particles
    this->loop(storage, stats);

    // sum up accumulated storage, compute statistics
    this->afterLoop(storage, stats);

    // integrate all equations
    equations.finalize(scheduler, storage, t);

    // apply boundary conditions after the loop
    bc->finalize(storage);

    // finalize all materials (integrate fragmentation model)
    for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
        PROFILE_SCOPE("SymmetricSolver finalize materials")
        MaterialView material = storage.getMaterial(i);
        material->finalize(scheduler, storage, material.sequence());
    }
}

template <Size Dim>
void SymmetricSolver<Dim>::create(Storage& storage, IMaterial& material) const {
    storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);

    // create necessary quantities
    equations.create(storage, material);

    // check equations and create quantities
    this->sanityCheck(storage);
}

template <Size Dim>
void SymmetricSolver<Dim>::loop(Storage& storage, Statistics& UNUSED(stats)) {
    MEASURE_SCOPE("SymmetricSolver::loop");
    // (re)build neighbour-finding structure; this needs to be done after all equations
    // are initialized in case some of them modify smoothing lengths
    ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder->build(scheduler, r);

    // here we use a kernel symmetrized in smoothing lengths:
    // \f$ W_ij(r_i - r_j, 0.5(h[i] + h[j]) \f$
    SymmetrizeSmoothingLengths<LutKernel<Dim>> symmetrizedKernel(kernel);

    auto functor = [this, r, &symmetrizedKernel](const Size i, ThreadData& data) {
        finder->findLowerRank(i, r[i][H] * kernel.radius(), data.neighs);
        data.grads.clear();
        data.idxs.clear();
        for (auto& n : data.neighs) {
            const Size j = n.index;
            const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
            ASSERT(hbar > EPS && hbar <= r[i][H], hbar, r[i][H]);
            if (getSqrLength(r[i] - r[j]) >= sqr(kernel.radius() * hbar)) {
                // aren't actual neighbours
                continue;
            }
            const Vector gr = symmetrizedKernel.grad(r[i], r[j]);
            ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) <= 0._f, gr, getLength(r[i] - r[j]));
            data.grads.emplaceBack(gr);
            data.idxs.emplaceBack(j);
        }
        data.derivatives.evalSymmetric(i, data.idxs, data.grads);
    };
    PROFILE_SCOPE("GenericSolver main loop");
    parallelFor(scheduler, threadData, 0, r.size(), functor);
}

template <Size Dim>
void SymmetricSolver<Dim>::beforeLoop(Storage& storage, Statistics& UNUSED(stats)) {
    // clear thread local storages
    PROFILE_SCOPE("GenericSolver::beforeLoop");
    for (ThreadData& data : threadData) {
        data.derivatives.initialize(storage);
    }
}

template <Size Dim>
void SymmetricSolver<Dim>::afterLoop(Storage& storage, Statistics& stats) {
    Accumulated* first = nullptr;

    // sum up thread local accumulated values
    Array<Accumulated*> threadLocalAccumulated;
    for (ThreadData& data : threadData) {
        if (!first) {
            first = &data.derivatives.getAccumulated();
        } else {
            ASSERT(first != nullptr);
            threadLocalAccumulated.push(&data.derivatives.getAccumulated());
        }
    }
    ASSERT(first != nullptr);
    first->sum(scheduler, threadLocalAccumulated);

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

template <Size Dim>
RawPtr<const IBasicFinder> SymmetricSolver<Dim>::getFinder(ArrayView<const Vector> r) {
    /// \todo same thing as in AsymmetricSolver -> move to shared parent?
    finder->build(scheduler, r);
    return &*finder;
}

template <Size Dim>
void SymmetricSolver<Dim>::sanityCheck(const Storage& UNUSED(storage)) const {
    // we must solve smoothing length somehow
    if (!equations.contains<AdaptiveSmoothingLength>() && !equations.contains<ConstSmoothingLength>()) {
        throw InvalidSetup(
            "No solver of smoothing length specified; add either ConstSmootingLength or "
            "AdaptiveSmootingLength into the list of equations");
    }
}

template class SymmetricSolver<1>;
template class SymmetricSolver<2>;
template class SymmetricSolver<3>;

NAMESPACE_SPH_END
