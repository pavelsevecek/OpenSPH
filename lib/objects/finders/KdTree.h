#pragma once

/// Implementation of the K-d tree finder using modified Nanoflann code
/// (https://github.com/jlblancoc/nanoflann)
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/finders/Finder.h"
#include "objects/finders/Nanoflann.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

/// Array of vectors to be used in nanoflann code
class PointCloud : public Object {
private:
    ArrayView<Vector> storage;

public:
    PointCloud() = default;

    PointCloud(ArrayView<Vector> storage)
        : storage(storage) {}

    INLINE int kdtree_get_point_count() const { return storage.size(); }

    INLINE Float kdtree_distance(const Vector& p1, const int idx2, int UNUSED(size)) const {
        return getSqrLength(storage[idx2] - p1);
    }

    INLINE Float kdtree_get_pt(const int idx, int n) const { return storage[idx][n]; }

    template <class BBOX>
    bool kdtree_get_bbox(BBOX& UNUSED(bb)) const {
        return false;
    }
};


class KdTree : public Abstract::Finder {
private:
    PointCloud cloud;
    using KdTreeImpl = KDTreeSingleIndexAdaptor<L2_Simple_Adaptor<Float, PointCloud>, PointCloud, 3, int>;
    Optional<KdTreeImpl> kdTree;


protected:
    virtual void buildImpl(ArrayView<Vector> values) override {
        cloud = PointCloud(values);
        kdTree.emplace(3, cloud, KDTreeSingleIndexAdaptorParams(1));
        kdTree->buildIndex();
    }

public:
    KdTree() = default;

    virtual int findNeighbours(const int index,
                               const Float radius,
                               Array<NeighbourRecord>& neighbours,
                               Flags<FinderFlags> flags = EMPTY_FLAGS,
                               const Float error        = 0._f) const override {
        neighbours.clear();
        SearchParams params;
        params.sorted = false;
        params.eps    = error;
        /// \todo flags
        (void)flags;
        const int n = kdTree->radiusSearch(values[index], radius, neighbours, params);
        return n;
    }
};

NAMESPACE_SPH_END
