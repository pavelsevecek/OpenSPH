#pragma once

#include "common/ForwardDecl.h"
#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

class IGravity;
class UniformRng;

enum class GalaxySettingsId {
    DISK_PARTICLE_COUNT,
    DISK_RADIAL_CUTOFF,
    DISK_RADIAL_SCALE,
    DISK_VERTICAL_SCALE,
    DISK_VERTICAL_CUTOFF,
    DISK_MASS,
    DISK_TOOMRE_Q,

    HALO_PARTICLE_COUNT,
    HALO_SCALE_LENGTH,
    HALO_GAMMA,
    HALO_CUTOFF,
    HALO_MASS,

    BULGE_PARTICLE_COUNT,
    BULGE_SCALE_LENGTH,
    BULGE_CUTOFF,
    BULGE_MASS,

    PARTICLE_RADIUS,
};

using GalaxySettings = Settings<GalaxySettingsId>;

namespace Galaxy {

enum class PartEnum {
    DISK,
    HALO,
    BULGE,
};

Storage generateDisk(UniformRng& rng, const GalaxySettings& settings);

Storage generateHalo(UniformRng& rng, const GalaxySettings& settings);

Storage generateBulge(UniformRng& rng, const GalaxySettings& settings);

struct IProgressCallbacks : public Polymorphic {
    /// \brief Called when computing new part of the galaxy (particle positions or velocities).
    virtual void onPart(const Storage& storage, const Size partId, const Size numParts) const = 0;
};

struct NullProgressCallbacks : public IProgressCallbacks {
    virtual void onPart(const Storage& UNUSED(storage),
        const Size UNUSED(partId),
        const Size UNUSED(numParts)) const override {}
};

Storage generateIc(const RunSettings& globals,
    const GalaxySettings& settings,
    const IProgressCallbacks& callbacks);

} // namespace Galaxy

NAMESPACE_SPH_END
