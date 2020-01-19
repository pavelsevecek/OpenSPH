#pragma once

/// \file UniformGrid.h
/// \brief Simple algorithm for finding nearest neighbours using spatial partitioning of particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2019

#include "objects/containers/LookupMap.h"
#include "objects/finders/NeighbourFinder.h"

NAMESPACE_SPH_BEGIN

/// \brief Finder projecting a uniform grid on the particles.
class UniformGridFinder : public FinderTemplate<UniformGridFinder> {
protected:
    LookupMap lut;

    /// Multiplier of the number of cells
    Float relativeCellCnt;

    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override;

public:
    /// \param relativeCellCnt Multiplier of the number of constructed voxels. The actual number scales with
    ///                        the number points; relative voxel count 1 means that on average, one cell will
    ///                        contain one particle. Higher value means more cells, therefore less than one
    ///                        particle per cell, etc.
    UniformGridFinder(const Float relativeCellCnt = 1);

    ~UniformGridFinder();

    template <bool FindAll>
    Size find(const Vector& pos, const Size index, const Float radius, Array<NeighbourRecord>& neighs) const;

    /// Exposed for gravity
    /// \todo refactor, finder shouldn't know anything about gravity

    template <typename TFunctor>
    void iterate(const TFunctor& functor) {
        const Size size = lut.getDimensionSize();
        for (Size z = 0; z < size; ++z) {
            for (Size y = 0; y < size; ++y) {
                for (Size x = 0; x < size; ++x) {
                    const Indices idxs = Indices(x, y, z);
                    Array<Size>& particles = lut(idxs);
                    functor(particles);
                }
            }
        }
    }

    INLINE Size getDimensionSize() const {
        return lut.getDimensionSize();
    }
};

NAMESPACE_SPH_END
