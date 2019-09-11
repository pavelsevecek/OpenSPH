#pragma once

#include "sph/boundary/Boundary.h"
#include "sph/kernel/Kernel.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

/// An Implicit SPH Formulation for Incompressible Linearly Elastic Solids
///
/// See \cite Peer_2017
class ElasticDeformationSolver : public ISolver {
private:
    AutoPtr<IBasicFinder> finder;

    /// Scheduler used to parallelize the solver.
    IScheduler& scheduler;

    Vector gravity;

    AutoPtr<IBoundaryCondition> bc;

    /// Selected SPH kernel
    SymmetrizeSmoothingLengths<LutKernel<DIMENSIONS>> kernel;

    /// Precomputed list of neighbours for each particle
    Array<Array<Size>> neighsPerParticle;

    Array<AffineMatrix> R;

    /// Initial (reference) positions
    Array<Vector> r0;

    /// Initial particle volumes
    Array<Float> V0;

public:
    ElasticDeformationSolver(IScheduler& scheduler,
        const RunSettings& settings,
        AutoPtr<IBoundaryCondition>&& bc);

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

NAMESPACE_SPH_END
