#pragma once

/// Implementation of the K-d tree finder using modified Nanoflann code
/// (https://github.com/jlblancoc/nanoflann)
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/finders/AbstractFinder.h"
#include "objects/finders/Nanoflann.h"
#include "objects/wrappers/Optional.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

/// Array of vectors to be used in nanoflann code
class PointCloud  {
private:
    ArrayView<Vector> storage;
    const Order* rankH;

public:
    /// \todo this is really ugly solution, solve it directly in nanoflann
    mutable Size refRank = std::numeric_limits<Size>::max(); // rank of the reference index

    PointCloud() = default;

    PointCloud(ArrayView<Vector> storage, const Order& rankH)
        : storage(storage)
        , rankH(&rankH) {}

    INLINE int kdtree_get_point_count() const { return storage.size(); }

    INLINE Float kdtree_distance(const Vector& v, const Size idx, int UNUSED(size)) const {
        ASSERT(refRank < std::numeric_limits<Size>::max());
        if (refRank > (*rankH)[idx]) {
            return getSqrLength(v - storage[idx]);
        } else {
            return INFTY;
        }
    }

    INLINE Float kdtree_get_pt(const Size idx, uint n) const { return storage[idx][n]; }

    template <class BBOX>
    bool kdtree_get_bbox(BBOX& UNUSED(bb)) const {
        return false;
    }
};


class KdTree : public Abstract::Finder {
private:
    PointCloud cloud;
    using KdTreeImpl = KDTreeSingleIndexAdaptor<L2_Simple_Adaptor<Float, PointCloud>, PointCloud, 3, Size>;
    Optional<KdTreeImpl> kdTree;


protected:
    virtual void buildImpl(ArrayView<Vector> values) override {
        PROFILE_SCOPE("KdTree::buildImpl")
        cloud = PointCloud(values, this->rankH);
        kdTree.emplace(3, cloud, KDTreeSingleIndexAdaptorParams(1));
        kdTree->buildIndex();
    }

public:
    KdTree() = default;

    virtual Size findNeighbours(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override {
        PROFILE_SCOPE("KdTree::findNeighbours")
        neighbours.clear();
        SearchParams params;
        params.sorted = false;
        params.eps = error;
        if (flags.has(FinderFlags::FIND_ONLY_SMALLER_H)) {
            cloud.refRank = rankH[index];
        } else {
            cloud.refRank = this->values.size(); // largest rank, all particles are smaller
        }
        return kdTree->radiusSearch(values[index], Math::sqr(radius), neighbours, params);
    }
};

NAMESPACE_SPH_END
