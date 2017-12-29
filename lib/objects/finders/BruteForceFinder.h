#pragma once

/// \file BruteForceFinder.h
/// \brief Object finding nearest neighbours by evaluating all partice pairs
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/finders/INeighbourFinder.h"
#include "objects/geometry/Box.h"

NAMESPACE_SPH_BEGIN

/// \brief Searches for neighbours by 'brute force', comparing every pair of vectors.
///
/// This implementation is not intended for usage in high-performance code, as computing all particle pairs is
/// too slow. Use other more efficient finders, such as \ref KdTree of \ref UniformGridFinder.
/// BruteForceFinder should be used only for testing and debugging purposes.
class BruteForceFinder : public INeighbourFinder {
protected:
    // no need to implement these for brute force
    virtual void buildImpl(ArrayView<const Vector> UNUSED(values)) override {}

    virtual void rebuildImpl(ArrayView<const Vector> UNUSED(values)) override {}

public:
    BruteForceFinder() = default;

    virtual Size findNeighbours(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlag> flags = EMPTY_FLAGS,
        const Float UNUSED(error) = 0._f) const override {
        neighbours.clear();
        const Size refRank =
            (flags.has(FinderFlag::FIND_ONLY_SMALLER_H)) ? this->rankH[index] : this->values.size();
        for (Size i = 0; i < this->values.size(); ++i) {
            const Float distSqr = getSqrLength(this->values[i] - this->values[index]);
            if (rankH[i] < refRank && distSqr < sqr(radius)) {
                neighbours.push(NeighbourRecord{ i, distSqr });
            }
        }
        return neighbours.size();
    }

    virtual Size findNeighbours(const Vector& position,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlag> UNUSED(flags) = EMPTY_FLAGS,
        const Float UNUSED(error) = 0._f) const override {
        neighbours.clear();
        for (Size i = 0; i < this->values.size(); ++i) {
            const Float distSqr = getSqrLength(this->values[i] - position);
            if (distSqr < sqr(radius)) {
                neighbours.push(NeighbourRecord{ i, distSqr });
            }
        }
        return neighbours.size();
    }

    /// Updates the structure when the position change.
    virtual void rebuild() {}
};

NAMESPACE_SPH_END
