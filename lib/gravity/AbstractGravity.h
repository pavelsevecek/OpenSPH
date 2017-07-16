#pragma once

/// \file AbstractGravity.h
/// \brief Base class for solvers of gravity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/finders/KdTree.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {

    /// \brief Interface for evaluators of gravitational interaction
    class Gravity {
    public:
        /// Builds the accelerating structure; needs to be called every time step.
        /// \ref points Positions of particles. Smoothing lengths must be strictly positive!
        /// \ref masses Particle masses, must be the same size as points
        virtual void build(ArrayView<const Vector> points, ArrayView<const Float> masses) = 0;

        /// Evaluates the gravitational acceleration of given particle.
        /// \param idx Index of particle in arrays given in \ref build function
        virtual Vector eval(const Size idx) = 0;

        /// Evaluates the gravitational acceleration at given point. The point must NOT correspond to any
        /// particle, as this case could formally lead to infinite acceleration if no smoothing kernel is
        /// used.
        /// \param r0 Point where the gravity is evaluated.
        virtual Vector eval(const Vector& r0) = 0;
    };
}

NAMESPACE_SPH_END
