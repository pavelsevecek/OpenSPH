#pragma once

/// \file GravitySolver.h
/// \brief SPH solver including gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "timestepping/ISolver.h"

NAMESPACE_SPH_BEGIN

class IGravity;
class IScheduler;
class EquationHolder;
class Accumulated;

/// \brief Extension of a generic SPH solver, including gravitational interactions of particles.
///
/// Explicitly specialized for \ref AsymmetricSolver, \ref SymmetricSolver and \ref EnergyConservingSolver.
template <typename TSphSolver>
class GravitySolver : public TSphSolver {
private:
    /// Implementation of gravity used by the solver
    AutoPtr<IGravity> gravity;

public:
    /// \brief Creates the gravity solver, used implementation of gravity given by settings parameters.
    GravitySolver(IScheduler& scheduler, const RunSettings& settings, const EquationHolder& equations);

    /// \brief Creates the gravity solver by explicitly specifying the gravity implementation.
    GravitySolver(IScheduler& scheduler,
        const RunSettings& settings,
        const EquationHolder& equations,
        AutoPtr<IGravity>&& gravity);

    ~GravitySolver();

protected:
    virtual void loop(Storage& storage, Statistics& stats) override;

    virtual void sanityCheck(const Storage& storage) const override;

    virtual RawPtr<const IBasicFinder> getFinder(ArrayView<const Vector> r) override;

    /// \brief Returns the accumulated storage, either thread-local or shared one based on SPH solver.
    Accumulated& getAccumulated();
};

NAMESPACE_SPH_END
