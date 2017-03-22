#pragma once

#include "objects/containers/Array.h"
#include "quantities/Quantity.h"
#include <map>

NAMESPACE_SPH_BEGIN


enum class AccumulatedIds {
    ACCELERATION,
    VELOCITY_DIVERGENCE,
    VELOCITY_ROTATION,
    VELOCITY_GRADIENT,
    /// Velocity gradient accumulated only for undamaged particles from the same body
    MATERIAL_VELOCITY_GRADIENT,
};

class Accumulated {
private:
    std::map<AccumulatedIds, Quantity> data;

public:
    Accumulated() = default;
};

NAMESPACE_SPH_END
