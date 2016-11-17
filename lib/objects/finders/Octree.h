#pragma once

/// Implementation of Octree algorithm for kNN queries.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz


#include "objects/containers/Array.h"
#include "geometry/Bounds.h"



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
