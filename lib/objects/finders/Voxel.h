#pragma once

/// \file Voxel.h
/// \brief Simple algorithm for finding nearest neighbours using spatial partitioning of particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2017

#include "objects/containers/LookupMap.h"
#include "objects/finders/AbstractFinder.h"

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
