#pragma once

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

namespace Post {

/// \brief Compares particle positions and state quantities of two storages.
///
/// If both states match, returns SUCCESS, otherwise returns an explanatory message.
/// This comparison method is mainly intended for SPH simulations.
/// \param test Particles to test
/// \param ref Reference particles
/// \param eps Allowed relative error.
Outcome compareParticles(const Storage& test, const Storage& ref, const Float eps = 1.e-6_f);

/// \brief Compares the positions, velocities and radii of the largest particles in given states.
///
/// If both states match, returns SUCCESS, otherwise returns an explanatory message.
/// This comparison method in suitable for N-body simulations with merging. Function considers two states
/// equal even if they differ by few particles.
/// \param test Particles to test
/// \param ref Reference particles
/// \param fraction Fraction of the largest particles to test. The rest is considered a "noise".
/// \param maxDeviation Maximal difference in positions of matching particles
/// \param eps Allowed relative error.
Outcome compareLargeSpheres(const Storage& test,
    const Storage& ref,
    const Float fraction,
    const Float maxDeviation,
    const Float eps = 1.e-6_f);

} // namespace Post

NAMESPACE_SPH_END
