#pragma once

/// \file AsymmetricSolver.h
/// \brief SPH solver with asymmetric particle evaluation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "sph/Material.h"
#include "sph/equations/Accumulated.h"
#include "sph/equations/EquationTerm.h"
#include "sph/equations/HelperTerms.h"
#include "sph/kernel/KernelFactory.h"
#include "system/Factory.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/ThreadLocal.h"
#include "timestepping/AbstractSolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Solver
///
/// \todo
class AsymmetricSolver : public Abstract::Solver {
protected:
    /// Holds all derivatives this thread computes
    DerivativeHolder derivatives;

    /// Cached array of neighbours, to avoid allocation every step
    Array<NeighbourRecord> neighs;

    /// Indices of real neighbours
    Array<Size> idxs;

    /// Cached array of gradients
    Array<Vector> grads;

    /// Thread pool used to parallelize the solver, runs the whole time the solver exists.
    SharedPtr<ThreadPool> pool;

    /// Selected granularity of the parallel processing. The more particles in simulation, the higher the
    /// value should be to utilize the solver optimally.
    Size granularity;

    /// Holds all equation terms evaluated by the solver.
    EquationHolder equations;

    /// Structure used to search for neighbouring particles
    AutoPtr<Abstract::Finder> finder;

    /// Selected SPH kernel, symmetrized over smoothing lenghs:
    /// \f$ W_ij(r_i - r_j, 0.5(h[i] + h[j]) \f$
    SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>> kernel;

public:
    AsymmetricSolver(const RunSettings& settings, const EquationHolder& eqs)
        : pool(makeShared<ThreadPool>(settings.get<int>(RunSettingsId::RUN_THREAD_CNT)))
        , threadData(*pool) {
        kernel = Factory::getKernel<DIMENSIONS>(settings);
        finder = Factory::getFinder(settings);
        granularity = settings.get<int>(RunSettingsId::RUN_THREAD_GRANULARITY);
        equations += eqs;
        // add term counting number of neighbours
        equations += makeTerm<NeighbourCountTerm>();
        // initialize all derivatives
        equations.setupThread(derivatives, settings);
    }

    virtual void integrate(Storage& storage, Statistics& stats) override {
        /// \todo move elsewhere
        storage.setThreadPool(pool);

        // initialize all materials (compute pressure, apply yielding and damage, ...)
        for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
            PROFILE_SCOPE("GenericSolver initialize materials")
            MaterialView material = storage.getMaterial(i);
            material->initialize(storage, material.sequence());
        }

        // initialize all equation terms (applies dependencies between quantities)
        equations.initialize(storage);

        // initialize accumulate storages & derivatives
        this->beforeLoop(storage, stats);

        // main loop over pairs of interacting particles
        this->loop(storage);

        // sum up accumulated storage, compute statistics
        this->afterLoop(storage, stats);

        // integrate all equations
        equations.finalize(storage);

        // finalize all materials (integrate fragmentation model)
        for (Size i = 0; i < storage.getMaterialCnt(); ++i) {
            PROFILE_SCOPE("GenericSolver finalize materials")
            MaterialView material = storage.getMaterial(i);
            material->finalize(storage, material.sequence());
        }
    }

    virtual void create(Storage& storage, Abstract::Material& material) const override {
        storage.insert<Size>(QuantityId::NEIGHBOUR_CNT, OrderEnum::ZERO, 0);
        equations.create(storage, material);
    }

protected:
    virtual void loop(Storage& storage) {
        // (re)build neighbour-finding structure; this needs to be done after all equations
        // are initialized in case some of them modify smoothing lengths
        ArrayView<Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        finder->build(r);

        auto functor = [this, r](const Size n1, const Size n2) {
            for (Size i = n1; i < n2; ++i) {
                finder->findNeighbours(i, maxH * kernel.radius(), data.neighs, EMPTY_FLAGS);
                grads.clear();
                idxs.clear();
                for (auto& n : data.neighs) {
                    const Size j = n.index;
                    const Float hbar = 0.5_f * (r[i][H] + r[j][H]);
                    ASSERT(hbar > EPS && hbar <= r[i][H], hbar, r[i][H]);
                    if (getSqrLength(r[i] - r[j]) >= sqr(this->kernel.radius() * hbar)) {
                        // aren't actual neighbours
                        continue;
                    }
                    const Vector gr = kernel.grad(r[i], r[j]);
                    ASSERT(isReal(gr) && dot(gr, r[i] - r[j]) < 0._f, gr, r[i] - r[j]);
                    grads.emplaceBack(gr);
                    idxs.emplaceBack(j);
                }
                data.derivatives.eval<false>(i, data.idxs, data.grads);
            }
        };
        PROFILE_SCOPE("AsymmetricSolver main loop");
        parallelFor(*pool, 0, r.size(), granularity, functor);
    }

    virtual void beforeLoop(Storage& storage, Statistics& UNUSED(stats)) {
        // clear thread accumulated
        PROFILE_SCOPE("GenericSolver::beforeLoop");
        derivatives.initialize(storage);
    }

    virtual void afterLoop(Storage& storage, Statistics& stats) {
        // store accumulated to storage
        Accumulated& accumulated = derivatives.getAccumulated();
        accumulated.store(storage);

        // compute neighbour statistics
        MinMaxMean neighs;
        ArrayView<Size> neighCnts = storage.getValue<Size>(QuantityId::NEIGHBOUR_CNT);
        const Size size = storage.getParticleCnt();
        for (Size i = 0; i < size; ++i) {
            neighs.accumulate(neighCnts[i]);
        }
        stats.set(StatisticsId::NEIGHBOUR_COUNT, neighs);
    }
};

NAMESPACE_SPH_END
