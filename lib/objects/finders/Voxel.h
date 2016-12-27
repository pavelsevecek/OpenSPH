#pragma once

/// Simple algorithm for finding nearest neighbours using spatial partitioning of particles
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/finders/AbstractFinder.h"
#include "objects/containers/LookupMap.h"

NAMESPACE_SPH_BEGIN

class LookupMap;

class VoxelFinder : public Abstract::Finder {
protected:
    LookupMap lut;

    virtual void buildImpl(ArrayView<Vector> values) override;

public:
    VoxelFinder();

    ~VoxelFinder();

    virtual int findNeighbours(const int index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override;

    /// Updates the structure when the position change.
    virtual void rebuild() {}
};

NAMESPACE_SPH_END
