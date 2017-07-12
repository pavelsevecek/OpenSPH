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
        /// \ref points Positions of particles
        /// \ref masses Particle masses, must be the same size as points
        virtual void build(ArrayView<const Vector> points, ArrayView<const Float> masses) = 0;

        /// Evaluates the gravitational acceleration of given particle.
        /// \param idx Index of particle in arrays given in \ref build function
        virtual Vector eval(const Size idx) = 0;
    };
}

NAMESPACE_SPH_END
