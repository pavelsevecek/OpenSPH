#pragma once

/// \file DensityIndependentSolver.h
/// \brief Density-independent formulation of SPH
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz
/// \date 2016-2021

#include "sph/equations/DerivativeHelpers.h"
#include "sph/equations/EquationTerm.h"
#include "sph/kernel/Kernel.h"
#include "thread/ThreadLocal.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

/// \brief Density-independent SPH solver
///
/// Uses solver of Saitoh & Makino \cite Saitoh_Makino_2013. Instead of density and specific energy,
/// independent variables are energy density (q) and internal energy of i-th particle (U). Otherwise, the
/// solver is similar to \ref SummationSolver; the energy density is computed using direct summation by
/// self-consistent solution with smoothing length.
///
/// \attention Works only for ideal gas EoS!
class DensityIndependentSolver : public ISolver {
private:
    IScheduler& scheduler;

    AutoPtr<IBasicFinder> finder;

    /// Holds all equations used by the solver
    EquationHolder equations;

    /// Holds all derivatives (shared for all threads)
    DerivativeHolder derivatives;

    SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>> kernel;

    struct ThreadData {
        /// Cached array of neighbours, to avoid allocation every step
        Array<NeighbourRecord> neighs;

        /// Indices of real neighbours
        Array<Size> idxs;

        /// Cached array of gradients
        Array<Vector> grads;
    };

    ThreadLocal<ThreadData> threadData;

public:
    explicit DensityIndependentSolver(IScheduler& scheduler, const RunSettings& settings);

    ~DensityIndependentSolver();

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

NAMESPACE_SPH_END
