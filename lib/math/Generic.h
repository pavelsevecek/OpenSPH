#pragma once

/// \todo move all generic function from math here

#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

/// Returns the components of the object in array.
template <typename T>
INLINE Array<Float> getComponents(const T& value) {
    // for floats and indices, return the value itself.
    return { (Float)value };
}

NAMESPACE_SPH_END
