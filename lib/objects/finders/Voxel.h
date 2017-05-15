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

    virtual void buildImpl(ArrayView<const Vector> points) override;

    virtual void rebuildImpl(ArrayView<const Vector> points) override;

public:
    VoxelFinder();

    ~VoxelFinder();

    virtual Size findNeighbours(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override;
		
	virtual Size findNeighbours(const Vector& position,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override;
		
private:
    Size findNeighboursImpl(const Vector& position, const Size refRank, const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags,
        const Float error) const;
		
};

NAMESPACE_SPH_END
