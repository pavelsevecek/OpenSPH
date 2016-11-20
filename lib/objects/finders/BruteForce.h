#pragma once

#include "geometry/Bounds.h"
#include "objects/finders/Finder.h"

NAMESPACE_SPH_BEGIN

/// Search for neighbours by 'brute force', comparing every pair of vectors. Useful for testing other finders.

class BruteForceFinder : public Abstract::Finder {
protected:
    virtual void buildImpl(ArrayView<Vector> UNUSED(values)) override {}

public:
    BruteForceFinder() = default;


    virtual int findNeighbours(const int index,
                               const Float radius,
                               Array<NeighbourRecord>& neighbours,
                               Flags<FinderFlags> flags  = EMPTY_FLAGS,
                               const Float UNUSED(error) = 0._f) const override {
        neighbours.clear();
        const int refRank =
            (flags.has(FinderFlags::FIND_ONLY_SMALLER_H)) ? this->rankH[index] : this->values.size();
        for (int i = 0; i < this->values.size(); ++i) {
            Float distSqr = getSqrLength(this->values[i] - this->values[index]);
            if (rankH[i] < refRank && distSqr < Math::sqr(radius)) {
                neighbours.push(NeighbourRecord{ i, distSqr });
            }
        }
        return neighbours.size();
    }


    /// Updates the structure when the position change.
    virtual void rebuild() {}
};

NAMESPACE_SPH_END