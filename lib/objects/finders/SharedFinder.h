#pragma once

/// \file SharedFinder.h
/// \brief Global instance of a finder that can be shared between different components of a run
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/finders/NeighbourFinder.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

/// \brief Singleton finder.
///
/// Currently instantiated for \ref KdTree and \ref UniformGrid.
template <typename TBase>
class SharedFinder : public ISymmetricFinder {
private:
    static AutoPtr<TBase> instance;

public:
    static TBase& getInstance();

    virtual Size findAll(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override;

    virtual Size findAll(const Vector& pos,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override;

    virtual Size findLowerRank(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override;

private:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override;
};

NAMESPACE_SPH_END
