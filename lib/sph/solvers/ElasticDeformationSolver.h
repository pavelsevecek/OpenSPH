#pragma once

#include "sph/kernel/Kernel.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

class ElasticDeformationSolver : public ISolver {
private:
    AutoPtr<IBasicFinder> finder;

    /// Scheduler used to parallelize the solver.
    IScheduler& scheduler;

    /// Selected SPH kernel
    LutKernel<DIMENSIONS> kernel;

    /// Precomputed list of neighbours for each particle
    Array<Array<Size>> neighsPerParticle;

    Array<Vector> w;

public:
    ElasticDeformationSolver(IScheduler& scheduler, const RunSettings& settings);

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void create(Storage& storage, IMaterial& material) const override;
};

NAMESPACE_SPH_END
