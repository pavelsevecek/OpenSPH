#pragma once

/// \file KdTree.h
/// \brief Implementation of the K-d tree finder using modified Nanoflann code
///        (https://github.com/jlblancoc/nanoflann)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017
/// \todo This implementation is not thread safe, cannot be used with parallelized solver!

#include "objects/finders/AbstractFinder.h"
#include "objects/wrappers/AutoPtr.h"

NAMESPACE_SPH_BEGIN

class KdTree : public Abstract::Finder {
private:
    struct Impl;
    AutoPtr<Impl> impl;

protected:
    virtual void buildImpl(ArrayView<const Vector> points) override;

    virtual void rebuildImpl(ArrayView<const Vector> points) override;

public:
    KdTree();

    ~KdTree();

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
};

NAMESPACE_SPH_END
