#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/Range.h"

NAMESPACE_SPH_BEGIN


/// Extension of array with bounds of elements. Bounds in no way affect adding or modifying elements of the
/// array, but can be accessed using getBounds() and used in clamp() method.
template <typename Type>
class LimitedArray : public Array<Type> {
private:
    Range bounds;

public:
    using Array<Type>::Array;

    /// Returns bounds of the array.
    const Range& getBounds() const {
        return  bounds;
    }

    /// Clamps i-th elements of the array by the bounds.
    void clamp(const int idx) {
        (*this)[idx] = Math::clamp((*this)[idx], bounds);
    }
};

NAMESPACE_SPH_END
