#pragma once

/// \file SummationSolver.h
/// \brief Solver using direction summation to compute density and smoothing length.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "sph/solvers/SymmetricSolver.h"

NAMESPACE_SPH_BEGIN

/// \brief SPH solver using density and specific energy as independent variables.
///
/// Density is solved by direct summation, using self-consistent solution with smoothing length. Energy is
/// evolved using energy equation.
class SummationSolver : public SymmetricSolver {
private:
    Size maxIteration;
    Float targetDensityDifference;
    bool adaptiveH;
    Array<Float> rho, h;

    LutKernel<DIMENSIONS> densityKernel;

public:
    SummationSolver(IScheduler& scheduler,
        const RunSettings& settings,
        const EquationHolder& additionalEquations = {});

    virtual void create(Storage& storage, IMaterial& material) const override;

private:
    virtual void beforeLoop(Storage& storage, Statistics& stats) override;

    virtual void sanityCheck(const Storage& storage) const override;
};

NAMESPACE_SPH_END
