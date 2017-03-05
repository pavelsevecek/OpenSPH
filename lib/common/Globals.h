#pragma once

/// Global parameters of the code.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

/// Precision used withing the code. Use Float instead of float or double where precision is important.
using Float = double;

/// Integral type used to index arrays (by default).
using Size = uint32_t;

/// Number of spatial dimensions using in the code.
constexpr int DIMENSIONS = 3;

/// Number of valid digits of numbers on output
constexpr int PRECISION = std::is_same<Float, double>::value ? 12 : 6;

/// Useful literal using defined precision. Use "1.0_f" instead of "1.0f" (float) or "1.0" (double) literals.
INLINE constexpr Float operator"" _f(const long double v) {
    return Float(v);
}

NAMESPACE_SPH_END
