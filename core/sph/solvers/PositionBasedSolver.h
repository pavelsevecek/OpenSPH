#pragma once

#include "sph/kernel/Kernel.h"
#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

class IGravity;

class PositionBasedSolver : public ISolver {
private:
    IScheduler& scheduler;
    AutoPtr<IBasicFinder> finder;
    AutoPtr<IGravity> gravity;

    LutKernel<3> poly6;
    LutKernel<3> spiky;

    Array<Array<Size>> neighbors;
    Array<Float> rho0;
    Array<Vector> drho1;
    Array<Float> lambda;
    Array<Vector> dp;

    Size iterCnt;
    Float eps = 1.e-10_f;

public:
    PositionBasedSolver(IScheduler& scheduler, const RunSettings& settings);

    virtual void integrate(Storage& storage, Statistics& stats) override;

    virtual void create(Storage& storage, IMaterial& material) const override;

private:
    void evalHydro(Storage& storage, Statistics& stats);

    void evalGravity(Storage& storage, Statistics& stats);

    void doIteration(Array<Vector>& r1, ArrayView<Float> rho1, ArrayView<const Float> m);
};

NAMESPACE_SPH_END
