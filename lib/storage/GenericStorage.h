#pragma once

#include "objects/containers/Array.h"
#include "storage/Iterate.h"

NAMESPACE_SPH_BEGIN


/// Base object for storing scalar, vector and tensor quantities of SPH particles.
class GenericStorage : public Noncopyable {
private:
    Array<Quantity> quantities;

public:
    GenericStorage() = default;

    int size() const { return quantities.size(); }

    void merge(GenericStorage& other) {
        // must contain the same quantities
        ASSERT(this->size() == other.size());
        iteratePair<IterateType::ALL>(quantities, other.quantities, [](auto&& ar1, auto&& ar2) {
            ar1.pushAll(ar2);
        });
    }

    /// \todo clone only time-dependent quantities
    GenericStorage clone() const {
        GenericStorage cloned;
        for (const Quantity& q : quantities) {
            cloned.quantities.push(q.clone());
        }
    }
};

/*template <>
INLINE decltype(auto) GenericStorage::view<QuantityType::SCALAR>(const int idx) {
    return scalars[idx];
}

template <>
INLINE decltype(auto) GenericStorage::view<QuantityType::VECTOR>(const int idx) {
    return vectors[idx];
}*/

/// \todo tensor


NAMESPACE_SPH_END
