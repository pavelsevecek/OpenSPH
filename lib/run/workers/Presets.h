#pragma once

#include "run/Node.h"

NAMESPACE_SPH_BEGIN

namespace Presets {

/// \brief Creates a node tree for basic collision simulation.
SharedPtr<WorkerNode> makeAsteroidCollision(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for collision simulation, consisting of stabilization of the target,
/// fragmentation phase and finally reaccumulation phase.
SharedPtr<WorkerNode> makeFragmentationAndReaccumulation(UniqueNameManager& nameMgr,
    const Size particleCnt = 10000);

/// \brief Creates a node tree for simulation of cratering.
SharedPtr<WorkerNode> makeCratering(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

/// \brief Creates a node tree for galaxy collision.
SharedPtr<WorkerNode> makeGalaxyCollision(UniqueNameManager& nameMgr, const Size particleCnt = 10000);

} // namespace Presets

NAMESPACE_SPH_END
