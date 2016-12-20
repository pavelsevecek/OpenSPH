#pragma once

#include "system/Settings.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Solver;
}

/// Creates a solver from parameters in settings.
INLINE std::unique_ptr<Abstract::Solver> getSolver(const GlobalSettings& settings);

NAMESPACE_SPH_END
