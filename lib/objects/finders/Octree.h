#pragma once

/// Implementation of Octree algorithm for kNN queries.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz


#include "objects/containers/Array.h"
#include "geometry/Box.h"
#include "objects/finders/AbstractFinder.h"

NAMESPACE_SPH_BEGIN

class Octree : public Abstract::Finder {
private:
    Box box;
    Array


protected:
    virtual void buildImpl(ArrayView<Vector> values) override {
        PROFILE_SCOPE("Octree::buildImpl")

    }

public:
    Octree() =  default;

    virtual int findNeighbours(const int index,
                               const Float radius,
                               Array<NeighbourRecord>& neighbours,
                               Flags<FinderFlags> flags = EMPTY_FLAGS,
                               const Float error        = 0._f) const override {
     }

};

NAMESPACE_SPH_END
