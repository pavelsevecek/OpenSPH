#pragma once

/// \file Globals.h
/// \brief Global parameters of the code
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

/// Precision used withing the code. Use Float instead of float or double where precision is important.
#ifdef SPH_SINGLE_PRECISION
using Float = float;
#else
using Float = double;
#endif

/// Integral type used to index arrays (by default).
using Size = uint32_t;

/// Signed integral type, used where negative numbers are necessary. Should match Size.
using SignedSize = int32_t;

/// Number of spatial dimensions in the code.
constexpr int DIMENSIONS = 3;

/// Number of valid digits of numbers on output
constexpr int PRECISION = std::is_same<Float, double>::value ? 12 : 6;

/// Useful literal using defined precision. Use "1.0_f" instead of "1.0f" (float) or "1.0" (double) literals.
INLINE constexpr Float operator"" _f(const long double v) {
    return Float(v);
}

constexpr char CODE_NAME[] = "OpenSPH";

NAMESPACE_SPH_END
