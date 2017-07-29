#pragma once

/// \file AbstractGravity.h
/// \brief Base class for solvers of gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/finders/KdTree.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {

    /// \brief Interface for evaluators of gravitational interaction
    class Gravity : public Polymorphic {
    public:
        /// Builds the accelerating structure; needs to be called every time step.
        virtual void build(const Storage& storage) = 0;

        /// Evaluates the gravitational acceleration for all particles in the storage.
        /// \param dv Acceleration values, may contain previous values, gravity should add acceleration
        ///           instead of replacing the previous values with new one.
        virtual void evalAll(ArrayView<Vector> dv, Statistics& stats) const = 0;

        /// Evaluates the gravitational acceleration at given point. The point must NOT correspond to any
        /// particle, as this case could formally lead to infinite acceleration if no smoothing kernel is
        /// used.
        /// \param r0 Point where the gravity is evaluated.
        virtual Vector eval(const Vector& r0, Statistics& stats) const = 0;
    };
}

NAMESPACE_SPH_END
