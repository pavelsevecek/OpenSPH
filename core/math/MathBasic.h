#pragma once

/// \file MathBasic.h
/// \brief Basic math routines
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/Object.h"

NAMESPACE_SPH_BEGIN

/// Minimum & Maximum value

template <typename T>
INLINE constexpr T min(const T& f1, const T& f2) {
    return (f1 < f2) ? f1 : f2;
}

template <typename T>
INLINE constexpr T max(const T& f1, const T& f2) {
    return (f1 > f2) ? f1 : f2;
}

template <typename T, typename... TArgs>
INLINE constexpr T min(const T& f1, const T& f2, const TArgs&... rest) {
    return min(min(f1, f2), rest...);
}

template <typename T, typename... TArgs>
INLINE constexpr T max(const T& f1, const T& f2, const TArgs&... rest) {
    return max(max(f1, f2), rest...);
}

template <typename T>
INLINE constexpr T clamp(const T& f, const T& f1, const T& f2) {
    return max(f1, min(f, f2));
}

template <typename T>
INLINE constexpr bool isOdd(const T& f) {
    return f & 1;
}

NAMESPACE_SPH_END
