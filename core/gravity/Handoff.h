#pragma once

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \brief Determines how to compute the radii of the spheres
enum class HandoffRadius {
    /// The created sphere has the same volume as the SPH particles (=mass/density)
    EQUAL_VOLUME,

    /// The radius is proportional to the smoothing length of the particles.
    SMOOTHING_LENGTH,
};

struct HandoffParams {
    /// Method for computing radius of solid spheres
    HandoffRadius radiusType = HandoffRadius::EQUAL_VOLUME;

    /// Multiplier of smoothing lengths, used if radiusType is set to SMOOTHING_LENGTH.
    Float smoothingLengthMult = 0.333_f;

    /// If true, high-energy particles are removed.
    bool removeSublimated = true;
};

/// \brief Converts smoothed particles to solid spheres, used as an input of N-body simulations.
Storage smoothedToSolidHandoff(const Storage& input, const HandoffParams& params);

/// \brief Merges overlapping spheres into a larger sphere with the same volume.
///
/// Function tries to preserve the surface of the bodies.
void mergeOverlappingSpheres(IScheduler& scheduler,
    Storage& storage,
    const Float surfacenessThreshold = 0.5_f,
    const Size numIterations = 3,
    const Size minComponentSize = 100);

NAMESPACE_SPH_END
