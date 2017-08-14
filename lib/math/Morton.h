#pragma once

#include "objects/geometry/Vector.h"

NAMESPACE_SPH_BEGIN

/// Calculates a 30-bit Morton code for the given vector located within the unit cube [0,1]. Input vector must be inside [0,1] cube, checked by assert.
Size morton(const Vector& v);


NAMESPACE_SPH_END
