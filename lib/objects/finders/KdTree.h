#pragma once

/// Implementation of the K-d tree finder using modified Nanoflann code
/// (https://github.com/jlblancoc/nanoflann)
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/finders/AbstractFinder.h"

NAMESPACE_SPH_BEGIN

class KdTree : public Abstract::Finder {
private:
    struct Impl;
    std::unique_ptr<Impl> impl;

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
};

NAMESPACE_SPH_END
