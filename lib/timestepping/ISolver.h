#pragma once

/// \file ISolver.h
/// \brief Base interface for all solvers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz
/// \date 2016-2018

#include "common/ForwardDecl.h"
#include "objects/geometry/Vector.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \brief Base class for all solvers.
///
/// This generic interface allows to use the code for any problems with explicit timestepping, meaning it is
/// SPH-agnostic. It can be used also for N-body simulations, etc. The solver computes derivatives of
/// time-dependent quantities and saves these derivatives to corresponding buffers in given \ref Storage
/// object. The temporal integration in then performed by time-stepping algorithm.
class ISolver : public Polymorphic {
public:
    /// \brief Computes derivatives of all time-dependent quantities.
    ///
    /// The solver can also modify the quantities arbitrarily. It is however not recommended to perform the
    /// integration in the solver (using the time step stored in \ref Statistics) as this is a job for
    /// timestepping. The solver can modify quantities using boundary conditions, inter-quantity relationships
    /// (such as the summation equation for density in SPH), clamping of values etc. It is also possible to
    /// add or remove particles in the storage and modify materials. Threads running concurrently with the
    /// solver must assume the solver can modify the storage at any time, so accessing the storage from
    /// different threads is only allowed before or after \ref integrate; the is no locking for performance
    /// reasons.
    ///
    /// All highest order derivatives are guaranteed to be set to zero when \ref integrate is called (this is
    /// a responsibility of \ref ITimeStepping implementation).
    ///
    /// \param storage Storage containing all quantities.
    /// \param stats Object where the solver saves all computed statistics of the run.
    virtual void integrate(Storage& storage, Statistics& stats) = 0;

    /// \brief Detects the collisions and computes new positions of particles.
    ///
    /// The positions and velocities of particles need to be tracked at the beginning of timestep, the actual
    /// step is done by the calling timestepping. Default implementation does not handle collisions, so the
    /// particle positions are simply advanced as \f[\vec r \mathrel{+}= \vec v {\rm d}t \,.\f]
    /// This is suitable for solvers with no concept of collision (for example collisions do not have to be
    /// handled explicitly in SPH, they are a result of solving equations of hydrodynamics). Function is
    /// executed for each drift timestep (may be called more than once in a single step).
    ///
    /// \param storage Storage containing all quantities.
    /// \param stats Object where the solver saves collision statistics.
    /// \param dt Drift timestep, or time interval in which the collisions should be detected. Note that this
    ///           timestep can be different than the one in statistics, depending on selected timestepping
    ///           algorithm (for example LeapFrog uses drift step dt/2).
    virtual void collide(Storage& UNUSED(storage), Statistics& UNUSED(stats), const Float UNUSED(dt)) {}

    /// \brief Initializes all quantities needed by the solver in the storage.
    ///
    /// When called, storage already contains particle positions and their masses. All remaining quantities
    /// must be created by the solver. The function is called once for every body in the run. The given
    /// storage is guaranteed to be homogeneous; it contains only a single material.
    ///
    /// Note that when settings up initial condition using \ref InitialConditions object, the instance of \ref
    /// ISolver used for creating quantities can be different than the one used during the run. It is not
    /// recommended to set up or modify member variables of the solver from \ref create function or keep a
    /// reference to the calling solver elsewhere.
    ///
    /// \param storage Particle storage that shall be modified as needed by the solver.
    /// \param material Material containing parameters of the body being created. The solver can also set up
    ///                 necessary timestepping parameters of the material (ranges and minimal values of
    ///                 quantities).
    virtual void create(Storage& storage, IMaterial& material) const = 0;
};


NAMESPACE_SPH_END
