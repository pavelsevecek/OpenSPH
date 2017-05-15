#include "objects/finders/KdTree.h"
#include "objects/finders/Nanoflann.h"
#include "objects/wrappers/Optional.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

/// Array of vectors to be used in nanoflann code
class PointCloud {
private:
    ArrayView<const Vector> storage;
    const Order* rankH;

public:
    /// \todo this is really ugly solution, solve it directly in nanoflann
    mutable Size refRank = std::numeric_limits<Size>::max(); // rank of the reference index

    PointCloud() = default;

    PointCloud(ArrayView<const Vector> storage, const Order& rankH)
        : storage(storage)
        , rankH(&rankH) {}

    INLINE int kdtree_get_point_count() const {
        return storage.size();
    }

    INLINE Float kdtree_distance(const Vector& v, const Size idx, int UNUSED(size)) const {
        ASSERT(refRank < std::numeric_limits<Size>::max());
        if (refRank > (*rankH)[idx]) {
            return getSqrLength(v - storage[idx]);
        } else {
            return LARGE;
        }
    }

    INLINE Float kdtree_get_pt(const Size idx, uint n) const {
        return storage[idx][n];
    }

    template <class BBOX>
    bool kdtree_get_bbox(BBOX& UNUSED(bb)) const {
        return false;
    }
};

struct KdTree::Impl {
    PointCloud cloud;
    using KdTreeAdaptor = KDTreeSingleIndexAdaptor<L2_Simple_Adaptor<Float, PointCloud>, PointCloud, 3, Size>;
    Optional<KdTreeAdaptor> kdTree;

    Impl(ArrayView<const Vector> points, Order& rankH) {
        cloud = PointCloud(points, rankH);
        kdTree.emplace(3, cloud, KDTreeSingleIndexAdaptorParams(1));
        kdTree->buildIndex();
    }
};

KdTree::KdTree() = default;

KdTree::~KdTree() = default;

void KdTree::buildImpl(ArrayView<const Vector> points) {
    PROFILE_SCOPE("KdTree::buildImpl")
    impl = makeAuto<KdTree::Impl>(points, this->rankH);
}

void KdTree::rebuildImpl(ArrayView<const Vector> points) {
    /// \todo optimize
    buildImpl(points);
}

Size KdTree::findNeighbours(const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> flags,
    const Float error) const {
    PROFILE_SCOPE("KdTree::findNeighbours")
    neighbours.clear();
    SearchParams params;
    params.sorted = false;
    params.eps = error;
    if (flags.has(FinderFlags::FIND_ONLY_SMALLER_H)) {
        impl->cloud.refRank = rankH[index];
    } else {
        impl->cloud.refRank = this->values.size(); // largest rank, all particles are smaller
    }
    return impl->kdTree->radiusSearch(values[index], sqr(radius), neighbours, params);
}

Size KdTree::findNeighbours(const Vector& position,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> UNUSED(flags),
    const Float error) const {
    PROFILE_SCOPE("KdTree::findNeighbours")
    neighbours.clear();
    SearchParams params;
    params.sorted = false;
    params.eps = error;
    impl->cloud.refRank = this->values.size();
    return impl->kdTree->radiusSearch(position, sqr(radius), neighbours, params);
}

NAMESPACE_SPH_END
