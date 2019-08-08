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

Storage generateIc(const RunSettings& globals, const GalaxySettings& settings);

} // namespace Galaxy

NAMESPACE_SPH_END
