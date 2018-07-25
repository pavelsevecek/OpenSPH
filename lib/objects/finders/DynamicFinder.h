#pragma once

/// \file DynamicFinder.h
/// \brief Finder switching between K-d tree and Voxel finder based on particle spatial distribution
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2018

#include "common/ForwardDecl.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

class DynamicFinder : public ISymmetricFinder {
private:
    AutoPtr<ISymmetricFinder> actual;

    /// Threshold for using voxel finder
    Float compactThreshold;

protected:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override;

public:
    explicit DynamicFinder(const RunSettings& settings);

    virtual Size findAll(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override;

    virtual Size findAll(const Vector& position,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override;

    virtual Size findLowerRank(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override;

    /// \brief Replaces the current finder with the most suitable one.
    ///
    /// The decision is based on empirical metric; for compact particle distributions (ball of particles,
    /// ...), the VoxelFinder is used, while for more scattered particles KdTree is used. If the selected
    /// finder matches the one currently used, the current is re-used (it is not destroyed and re-created).
    ///
    /// Does not have to be called manually, the function is called from (re)build function. Exposed mainly
    /// for testing purposes.
    /// \return The value of the empirical metric; lower value than compactThreshold will result in
    ///         VoxelFinder being selected, otherwise KdTree is used.
    Float updateFinder(ArrayView<const Vector> points);
};

NAMESPACE_SPH_END
