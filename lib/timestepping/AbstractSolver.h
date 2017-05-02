#pragma once

/// \file AbstractSolver.h
/// \brief Base interface for all solvers
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz
/// \date 2016-2017

#include "common/ForwardDecl.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {

    /// Base class for all solvers. Does not require SPH discretization, can be also used for N-body
    /// simulations, ...
    class Solver : public Polymorphic {
    public:
        /// Computes derivatives of all time-dependent quantities.
        /// \param storage Storage containing all quantities. All highest order derivatives are guaranteed to
        ///        be set to zero (this is responsibility of TimeStepping).
        /// \param stats Object where the solver saves all computes statistics of the run.
        virtual void integrate(Storage& storage, Statistics& stats) = 0;

        /// Initialize all quantities needed by the solver in the storage. When called, storage must already
        /// contain particle positions and their masses. All remaining quantities must be created by the
        /// solver.
        virtual void create(Storage& storage, Abstract::Material& material) const = 0;
    };
}

NAMESPACE_SPH_END
