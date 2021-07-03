#pragma once

#include "run/Node.h"

NAMESPACE_SPH_BEGIN

namespace Presets {

enum class Id {
    COLLISION,
    FRAGMENTATION_REACCUMULATION,
    CRATERING,
    PLANETESIMAL_MERGING,
    GALAXY_COLLISION,
    ACCRETION_DISK,
    SOLAR_SYSTEM,
};

/// \brief Creates a node tree for the preset with given ID.
SharedPtr<JobNode> make(const Id id, UniqueNameManager& nameMgr, const Size particleCnt = 100000);

/// \brief Creates a node tree for basic collision simulation.
SharedPtr<JobNode> makeAsteroidCollision(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for collision simulation, consisting of stabilization of the target,
/// fragmentation phase and finally reaccumulation phase.
SharedPtr<JobNode> makeFragmentationAndReaccumulation(UniqueNameManager& nameMgr,
    const Size particleCnt = 10000);

/// \brief Creates a node tree for simulation of cratering.
SharedPtr<JobNode> makeCratering(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for simulation of planetesimal collision.
SharedPtr<JobNode> makePlanetesimalMerging(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for galaxy collision.
SharedPtr<JobNode> makeGalaxyCollision(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for accretion simulation
SharedPtr<JobNode> makeAccretionDisk(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for the Solar System
SharedPtr<JobNode> makeSolarSystem(UniqueNameManager& nameMgr);

} // namespace Presets

NAMESPACE_SPH_END
