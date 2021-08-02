#pragma once

/// \file SymmetricSolver.h
/// \brief Basic SPH solver, evaluating all interactions symmetrically
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

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
/// default, no equations are evaluated, except for a 'dummy equation' counting number of neighbors. All
/// equations are evaluated symmetrically, meaning each particle pair is visited (at most) once and the
/// derivatives of quantities are computed for both particles at once. All derivatives computed by the solver
/// must be thus symmetric (i.e. derived from \ref SymmetricDerivative).
/// Symmetric evaluation allows faster computation, at cost of higher memory overhead (each thread has its own
/// buffers where the computed derivatives are accumulated) and cannot be use when more than one pass over
/// particle neighbors is needed to compute the derivative (unless the user constructs two SymmetricSolvers
/// with different sets of equations).
template <Size Dim>
class SymmetricSolver : public ISolver {
protected:
    struct ThreadData {
        /// Holds all derivatives this thread computes
        DerivativeHolder derivatives;

        /// Cached array of neighbors, to avoid allocation every step
        Array<NeighborRecord> neighs;

        /// Indices of real neighbors
        Array<Size> idxs;

        /// Cached array of gradients
        Array<Vector> grads;
    };

    /// Scheduler to parallelize the solver.
    IScheduler& scheduler;

    /// Thread-local structure caching all buffers needed to compute derivatives.
    ThreadLocal<ThreadData> threadData;

    /// Holds all equation terms evaluated by the solver.
    EquationHolder equations;

    /// Boundary condition used by the solver.
    AutoPtr<IBoundaryCondition> bc;

    /// Structure used to search for neighboring particles
    AutoPtr<ISymmetricFinder> finder;

    /// Selected SPH kernel
    LutKernel<Dim> kernel;

public:
    /// \brief Creates a symmetric solver, given the list of equations to solve
    ///
    /// Constructor may throw if the list of equations is not consistent with the solver.
    /// \param scheduler Scheduler used for parallelization.
    /// \param settings Settings containing parameter of the solver (SPH kernel used, etc.)
    /// \param eqs List of equations to solve.
    /// \param bc Boundary conditions used during the simulation.
    SymmetricSolver(IScheduler& scheduler,
        const RunSettings& settings,
        const EquationHolder& eqs,
        AutoPtr<IBoundaryCondition>&& bc);

    /// \brief Creates a symmetric solver, using boundary conditions specified in settings.
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

    virtual RawPtr<const IBasicFinder> getFinder(ArrayView<const Vector> r);

    /// \brief Used to check internal consistency of the solver.
    ///
    /// Ran when the solver is created. Function throws an exception if there are conflicting equations or the
    /// solver cannot solve given set of equations for some reason.
    virtual void sanityCheck(const Storage& storage) const;
};

NAMESPACE_SPH_END
