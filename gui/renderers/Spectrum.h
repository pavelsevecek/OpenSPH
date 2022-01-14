#pragma once

#include "gui/objects/Palette.h"

NAMESPACE_SPH_BEGIN

/// \brief Returns palette with colors for black body emission for given temperature.
///
/// The temperature is specified in Kelvins.
/// \param range Range of input temperatures.
Palette getBlackBodyPalette(const Interval range, const Size pointCnt);

Palette getEmissionPalette(const Interval range, const Size pointCnt);

NAMESPACE_SPH_END
