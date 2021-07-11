#pragma once

/// \file IGravity.h
/// \brief Base class for solvers of gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/ForwardDecl.h"
#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

/// \brief Interface for computing gravitational interactions of particles.
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
    /// \param dv Acceleration values; it may already contain some accelerations computed by other code
    ///           components, gravity should add acceleration instead of replacing the current values.
    /// \param stats Output statistics of the gravitational solver.
    virtual void evalAll(IScheduler& scheduler, ArrayView<Vector> dv, Statistics& stats) const = 0;

    /// \brief Evaluates the gravitational acceleration at given point.
    ///
    /// The point must NOT correspond to any particle, as this case could formally lead to infinite
    /// acceleration if no smoothing kernel is used.
    /// \param r0 Point where the gravity is evaluated.
    virtual Vector eval(const Vector& r0) const = 0;

    /// \brief Computes the total potential energy of the particles.
    ///
    /// The zero point is implementation-specific; it is not required that the energy is strictly negative.
    /// \param scheduler Scheduler used for parallelization.
    /// \param stats Output statistics of the gravitational solver.
    virtual Float evalEnergy(IScheduler& scheduler, Statistics& stats) const = 0;

    /// \brief Optionally returns a finder used by the gravity implementation.
    ///
    /// If the gravity uses an acceleration structure that implements the \ref IBasicFinder interface, this
    /// function allows the user to obtain the object and re-use in other parts of the code. Finder is assumed
    /// to be initialized after \ref build is called.
    ///
    /// If the gravity does not use any such structure or it simply does not implement the \ref IBasicFinder
    /// interface, the function returns nullptr.
    virtual RawPtr<const IBasicFinder> getFinder() const = 0;
};

NAMESPACE_SPH_END
