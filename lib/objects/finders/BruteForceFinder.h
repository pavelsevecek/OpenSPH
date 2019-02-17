#pragma once

/// \file BruteForceFinder.h
/// \brief Object finding nearest neighbours by evaluating all partice pairs
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Box.h"

NAMESPACE_SPH_BEGIN

/// \brief Searches for neighbours by 'brute force', comparing every pair of vectors.
///
/// This implementation is not intended for usage in high-performance code, as computing all particle pairs is
/// too slow. Use other more efficient finders, such as \ref KdTree of \ref UniformGridFinder.
/// BruteForceFinder should be used only for testing and debugging purposes.
class BruteForceFinder : public FinderTemplate<BruteForceFinder> {
protected:
    // no need to implement these for brute force
    virtual void buildImpl(IScheduler& UNUSED(scheduler), ArrayView<const Vector> UNUSED(values)) override {}

public:
    template <bool FindAll>
    Size find(const Vector& pos, const Size index, const Float radius, Array<NeighbourRecord>& neighs) const {
        for (Size i = 0; i < this->values.size(); ++i) {
            const Float distSqr = getSqrLength(this->values[i] - pos);
            if (distSqr < sqr(radius) && (FindAll || rank[i] < rank[index])) {
                neighs.push(NeighbourRecord{ i, distSqr });
            }
        }
        return neighs.size();
    }
};

NAMESPACE_SPH_END
