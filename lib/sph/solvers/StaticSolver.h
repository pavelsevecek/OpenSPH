#pragma once

/// \file StaticSolver.h
/// \brief Computes quantities to get equilibrium state
/// \author Pavel Sevecek
/// \date 2016-2017

#include "math/SparseMatrix.h"
#include "objects/wrappers/Outcome.h"
#include "sph/equations/EquationTerm.h"
#include "sph/solvers/GenericSolver.h"

NAMESPACE_SPH_BEGIN

#ifdef SPH_USE_EIGEN

/// Solves for total stress tensor sigma. Equations to be solved cannot be specified at the moment, that would
/// require a lot of extra work and it is not needed at the moment. Will be possibly extended in the future.
class StaticSolver {
private:
    AutoPtr<Abstract::Finder> finder;

    SymmetrizeSmoothingLengths<LutKernel<3>> kernel;

    GenericSolver equationSolver;

    Size boundaryThreshold;

    SparseMatrix matrix;

public:
    /// Constructs the solver.
    /// \param equations Additional forces. The forces can depend on spatial derivatives, but must be
    ///                  independent on both pressure and deviatoric stress. All quantities appearing in these
    ///                  equations are considered parameters of the problem, solver cannot be used to solve
    ///                  other quantities than the total stress tensor.
    StaticSolver(const RunSettings& settings, const EquationHolder& equations);

    /// Computed pressure and deviatoric stress are placed into the storage, overriding previously stored
    /// values. Values of internal energy are computed using equation of state.
    Outcome solve(Storage& storage, Statistics& stats);

    /// Creates all the necessary quantities in the storage.
    void create(Storage& storage, Abstract::Material& material);
};

#endif

NAMESPACE_SPH_END
