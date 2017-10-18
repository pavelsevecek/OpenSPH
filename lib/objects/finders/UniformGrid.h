#pragma once

/// \file Voxel.h
/// \brief Simple algorithm for finding nearest neighbours using spatial partitioning of particles
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2017

#include "objects/containers/LookupMap.h"
#include "objects/finders/INeighbourFinder.h"

NAMESPACE_SPH_BEGIN

/// \brief Finder projecting a uniform grid on the particles.
class UniformGridFinder : public INeighbourFinder {
protected:
    LookupMap lut;

    /// Multiplier of the number of cells
    Float relativeCellCnt;

    virtual void buildImpl(ArrayView<const Vector> points) override;

    virtual void rebuildImpl(ArrayView<const Vector> points) override;

public:
    /// \param relativeCellCnt Multiplier of the number of constructed voxels. The actual number scales with
    ///                        the number points; relative voxel count 1 means that on average, one cell will
    ///                        contain one particle. Higher value means more cells, therefore less than one
    ///                        particle per cell, etc.
    UniformGridFinder(const Float relativeCellCnt = 1);

    ~UniformGridFinder();

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

private:
    Size findNeighboursImpl(const Vector& position,
        const Vector& refPosition,
        const Size refRank,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags,
        const Float error) const;
};

NAMESPACE_SPH_END
