#pragma once

/// \file IGravity.h
/// \brief Base class for solvers of gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/ForwardDecl.h"
#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

/// \brief Interface for evaluators of gravitational interaction
class IGravity : public Polymorphic {
public:
    /// \brief Builds the accelerating structure.
    ///
    /// Needs to be called every time step.
    /// \param scheduler Scheduler used for parallelization of the build. Use \ref SEQUENTAIL for sequential
    ///                 (single-threaded) execution.
    virtual void build(IScheduler& scheduler, const Storage& storage) = 0;

    /// \brief Evaluates the gravitational acceleration concurrently.
    ///
    /// The function is blocking, it must exit after the gravity is evaluated.
    /// \param scheduler Scheduler used for parallelization.
    /// \param dv Acceleration values, may contain previous values, gravity should add acceleration
    ///           instead of replacing the previous values.
    /// \param stats Output statistics of the gravitational solver.
    virtual void evalAll(IScheduler& scheduler, ArrayView<Vector> dv, Statistics& stats) const = 0;

    /// \brief Evaluates the gravitational acceleration at given point.
    ///
    /// The point must NOT correspond to any particle, as this case could formally lead to infinite
    /// acceleration if no smoothing kernel is used.
    /// \param r0 Point where the gravity is evaluated.
    virtual Vector eval(const Vector& r0, Statistics& stats) const = 0;
};

NAMESPACE_SPH_END
