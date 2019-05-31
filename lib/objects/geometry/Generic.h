#pragma once

/// \file Generic.h
/// \brief Function for generic manipulation with geometric types
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni)
/// \date 2016-2019

#include "math/MathUtils.h"
#include "objects/containers/StaticArray.h"

NAMESPACE_SPH_BEGIN

/// \brief Returns a norm, absolute value by default.
template <typename T>
INLINE Float norm(const T& value) {
    return Sph::abs(value);
}

/// \brief Squared value of the norm.
template <typename T>
INLINE Float normSqr(const T& value) {
    return sqr(value);
}

/// \brief Returns maximum element, simply the value iself by default.
///
/// This function is intended for vectors and tensors, function for float is only for writing generic code.
template <typename T>
INLINE Float maxElement(const T& value) {
    return value;
}

/// \brief Returns minimum element, simply the value iself by default.
///
/// This function is intended for vectors and tensors, function for float is only for writing generic code.
template <typename T>
INLINE Float minElement(const T& value) {
    return value;
}

/// \brief Checks for nans and infs.
template <typename T>
INLINE bool isReal(const T& value) {
    return !std::isnan(value) && !std::isinf(value);
}

/// \brief Compares two objects of the same time component-wise.
///
/// Returns object containing components 0 or 1, depending whether components of the first objects are smaller
/// than components of the second object.
/// \note The return type can be generally different if the mask cannot be represented using type T.
template <typename T>
INLINE constexpr auto less(const T& v1, const T& v2) {
    return v1 < v2 ? T(1) : T(0);
}

/// \brief Returns the components of the object in array.
template <typename T>
INLINE StaticArray<Float, 6> getComponents(const T& value) {
    // for floats and indices, return the value itself.
    return { (Float)value };
}

NAMESPACE_SPH_END
