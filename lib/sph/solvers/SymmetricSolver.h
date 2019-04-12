#pragma once

/// \file SymmetricSolver.h
/// \brief Basic SPH solver, evaluating all interactions symmetrically
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "sph/equations/Derivative.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"
#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

class IBoundaryCondition;
class ISymmetricFinder;

/// \brief Basic solver for integration of SPH equations
///
/// The solver takes an array of equation terms and evaluate them, using compute gradients of SPH kernel. By
/// default, no equations are evaluated, except for a 'dummy equation' counting number of neighbours. All
/// equations are evaluated symmetrically, meaning each particle pair is visited (at most) once and the
/// derivatives of quantities are computed for both particles at once. All derivatives computed by the solver
/// must be thus symmetric (i.e. derived from \ref SymmetricDerivative).
/// Symmetric evaluation allows faster computation, at cost of higher memory overhead (each thread has its own
/// buffers where the computed derivatives are accumulated) and cannot be use when more than one pass over
/// particle neighbours is needed to compute the derivative (unless the user constructs two SymmetricSolvers
/// with different sets of equations).
class SymmetricSolver : public ISolver {
protected:
    struct ThreadData {
        /// Holds all derivatives this thread computes
        DerivativeHolder derivatives;

        /// Cached array of neighbours, to avoid allocation every step
        Array<NeighbourRecord> neighs;

        /// Indices of real neighbours
        Array<Size> idxs;

        /// Cached array of gradients
        Array<Vector> grads;
    };

    /// Scheduler to parallelize the solver.
    IScheduler& scheduler;

    /// Selected granularity of the parallel processing. The more particles in simulation, the higher the
    /// value should be to utilize the solver optimally.
    Size granularity;

    /// Thread-local structure caching all buffers needed to compute derivatives.
    ThreadLocal<ThreadData> threadData;

    /// Holds all equation terms evaluated by the solver.
    EquationHolder equations;

    /// Boundary condition used by the solver.
    AutoPtr<IBoundaryCondition> bc;

    /// Structure used to search for neighbouring particles
    AutoPtr<ISymmetricFinder> finder;

    /// Selected SPH kernel
    LutKernel<DIMENSIONS> kernel;

public:
    /// \brief Creates the symmetric solver, given the list of equations to solve
    ///
    /// Constructor may throw if the list of equations is not consistent with the solver.
    /// \param scheduler Scheduler used for parallelization.
    /// \param settings Settings containing parameter of the solver (SPH kernel used, etc.)
    /// \param eqs List of equations to solve.
    SymmetricSolver(IScheduler& scheduler, const RunSettings& settings, const EquationHolder& eqs);

    ~SymmetricSolver();

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void create(Storage& storage, IMaterial& material) const override;

protected:
    virtual void loop(Storage& storage, Statistics& stats);

    /// \todo there functions HAVE to be called, so it doesn't make sense to have them virtual - instead split
    /// it into a mandatory function and another one (empty by default) than can be overriden
    virtual void beforeLoop(Storage& storage, Statistics& stats);

    virtual void afterLoop(Storage& storage, Statistics& stats);

    virtual const IBasicFinder& getFinder(ArrayView<const Vector> r);

    /// \brief Used to check internal consistency of the solver.
    ///
    /// Ran when the solver is created. Function throws an exception if there are conflicting equations or the
    /// solver cannot solve given set of equations for some reason.
    virtual void sanityCheck(const Storage& storage) const;
};

NAMESPACE_SPH_END
