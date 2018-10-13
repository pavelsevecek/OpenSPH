#pragma once

/// \file Handoff.h
/// \brief Utility functions for handing-off the results of SPH simulations to N-body integrator
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \todo partially duplicates the PkdgravOutput

struct HandoffParams {

    /// \brief Determines how to compute the radii of the spheres
    enum class Radius {
        /// The created sphere has the same volume as the SPH particles (=mass/density)
        EQUAL_VOLUME,

        /// The radius is proportional to the smoothing length of the particles.
        SMOOTHING_LENGTH,

    };

    Radius radius = Radius::EQUAL_VOLUME;

    /// \brief Conversion factor between smoothing length and particle radius.
    ///
    /// Used only for Radius::SMOOTHING_LENGTH.
    Float radiusMultiplier = 0.333_f;

    /// \brief Threshold energy for removal of SPH particles.
    Float sublimationEnergy = LARGE;

    /// \brief If true, the particles are moved to a system where the center of mass is at the origin.
    bool centerOfMassSystem = false;


    struct LargestRemnant {

        /// \brief New number of particles in the largest remnant.
        ///
        /// By default, the number of particles stays the same.
        Optional<Size> particleOverride;

        /// Distribution used to generate particles inside the body
        ///
        /// \todo should this be optional? We need particles in close packing (no voids, no overlaps).
        AutoPtr<IDistribution> distribution;
    };

    /// \todo
    /// Separates the largest remnant. Other SPH particles are converted into N-body particles using 1-1
    /// correspondence, regenerated inside remnant with optionally lower particles density.

    Optional<LargestRemnant> largestRemnant;
};

Storage convertSphToSpheres(const Storage& sph, const RunSettings& settings, const HandoffParams& params);

NAMESPACE_SPH_END
