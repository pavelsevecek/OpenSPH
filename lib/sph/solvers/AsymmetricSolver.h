#pragma once

/// \file AsymmetricSolver.h
/// \brief SPH solver with asymmetric particle evaluation
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "sph/equations/Derivative.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"
#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Generic SPH solver, evaluating equations for each particle separately.
///
/// Contrary to SymmetricSolver, this solver computes the derivatives for each particle separately, meaning
/// each particle pair is evaluated twice (or not at all, of course). This leads to generally slower
/// evaluation, as we have to compute twice as much interactions. However, the solver is more versatile and
/// can solve equations with asymmetric derivatives. This allows to use helper terms (like strain rate
/// correction tensor) that require two passes over the particle neighbours; the derivative is completed after
/// the neighbours are summed up, as the list of neighbours contains ALL of the neighbouring particles (as
/// opposed to SymmetricSolver, where the list of neighbours only contains the particles with lower smoothing
/// length).
///
/// Another benefit is lower memory overhead (all threads can write into the same buffers) and
/// generally higher CPU usage.
class AsymmetricSolver : public ISolver {
protected:
    /// Holds all derivatives (shared for all threads)
    DerivativeHolder derivatives;

    /// Scheduler used to parallelize the solver.
    IScheduler& scheduler;

    struct ThreadData {
        /// Cached array of neighbours, to avoid allocation every step
        Array<NeighbourRecord> neighs;

        /// Indices of real neighbours
        Array<Size> idxs;

        /// Cached array of gradients
        Array<Vector> grads;
    };

    ThreadLocal<ThreadData> threadData;

    /// Selected granularity of the parallel processing. The more particles in simulation, the higher the
    /// value should be to utilize the solver optimally.
    Size granularity;

    /// Holds all equation terms evaluated by the solver.
    EquationHolder equations;

    /// Structure used to search for neighbouring particles
    AutoPtr<IBasicFinder> finder;

    /// Selected SPH kernel
    LutKernel<DIMENSIONS> kernel;

public:
    AsymmetricSolver(IScheduler& scheduler, const RunSettings& settings, const EquationHolder& eqs);

    ~AsymmetricSolver();

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void create(Storage& storage, IMaterial& material) const override;

protected:
    virtual void loop(Storage& storage, Statistics& stats);

    virtual void afterLoop(Storage& storage, Statistics& stats);

    /// Little hack to allow using different finders by derived classes.
    /// \todo generalize somehow
    virtual const IBasicFinder& getFinder(ArrayView<const Vector> r);

    virtual void sanityCheck(const Storage& storage) const;
};

NAMESPACE_SPH_END
