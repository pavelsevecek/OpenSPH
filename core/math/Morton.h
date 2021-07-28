#pragma once

#include "objects/geometry/Box.h"

NAMESPACE_SPH_BEGIN

/// \brief Calculates a 30-bit Morton code for the given vector located within the unit cube [0,1].
///
/// Input vector must be inside [0,1] cube, checked by assert.
Size morton(const Vector& v);

/// \brief Calculates the Morton code for a vector in specified box.
Size morton(const Vector& v, const Box& box);

/// \brief Reorders the input view so that neighboring points are close to each other in memory.
void spatialSort(ArrayView<Vector> points);

NAMESPACE_SPH_END
