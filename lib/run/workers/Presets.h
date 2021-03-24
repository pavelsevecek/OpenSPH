#pragma once

#include "run/Node.h"

NAMESPACE_SPH_BEGIN

namespace Presets {

/// \brief Creates a node tree for basic collision simulation.
SharedPtr<JobNode> makeAsteroidCollision(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for collision simulation, consisting of stabilization of the target,
/// fragmentation phase and finally reaccumulation phase.
SharedPtr<JobNode> makeFragmentationAndReaccumulation(UniqueNameManager& nameMgr,
    const Size particleCnt = 10000);

/// \brief Creates a node tree for simulation of cratering.
SharedPtr<JobNode> makeCratering(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for galaxy collision.
SharedPtr<JobNode> makeGalaxyCollision(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for accretion simulation
SharedPtr<JobNode> makeAccretionDisk(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

} // namespace Presets

NAMESPACE_SPH_END
