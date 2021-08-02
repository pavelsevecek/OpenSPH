#include "objects/finders/HashMapFinder.h"
#include "objects/geometry/Sphere.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

HashMapFinder::Cell::Cell() = default;

HashMapFinder::Cell::~Cell() = default;

HashMapFinder::HashMapFinder(const RunSettings& settings, const Float cellMult)
    : cellMult(cellMult) {
    kernelRadius = Factory::getKernel<3>(settings).radius();
}

HashMapFinder::~HashMapFinder() = default;

void HashMapFinder::buildImpl(IScheduler& UNUSED(scheduler), ArrayView<const Vector> points) {
    map.clear();
    cellSize = 0._f;
    for (Size i = 0; i < points.size(); ++i) {
        cellSize = max(cellSize, kernelRadius * points[i][H]);
    }
    cellSize *= cellMult;

    for (Size i = 0; i < points.size(); ++i) {
        const Indices idxs = floor(points[i] / cellSize);
        Cell& cell = map[idxs];
        cell.points.push(i);
        cell.box.extend(points[i]);
    }
}

template <bool FindAll>
Size HashMapFinder::find(const Vector& pos,
    const Size index,
    const Float radius,
    Array<NeighborRecord>& neighs) const {
    SPH_ASSERT(neighs.empty());
    const Indices idxs0 = floor(pos / cellSize);
    Sphere sphere(pos, radius);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                const Indices idxs = idxs0 + Indices(x, y, z);
                const auto iter = map.find(idxs);
                if (iter != map.end()) {
                    if (!sphere.overlaps(iter->second.box)) {
                        continue;
                    }
                    const Array<Size>& cell = iter->second.points;
                    for (Size i : cell) {
                        const Float distSqr = getSqrLength(values[i] - pos);
                        if (distSqr < sqr(radius) && (FindAll || rank[i] < rank[index])) {
                            neighs.emplaceBack(NeighborRecord{ i, distSqr });
                        }
                    }
                }
            }
        }
    }
    return neighs.size();
}

Outcome HashMapFinder::good(const Size maxBucketSize) const {
    for (Size i = 0; i < map.bucket_count(); ++i) {
        if (map.bucket_size(i) > maxBucketSize) {
            return makeFailed("Inefficient hash map: Bucket ", i, " has ", map.bucket_size(i), " elements");
        }
    }
    return SUCCESS;
}

MinMaxMean HashMapFinder::getBucketStats() const {
    MinMaxMean stats;
    for (Size i = 0; i < map.bucket_count(); ++i) {
        stats.accumulate(map.bucket_size(i));
    }
    return stats;
}

template Size HashMapFinder::find<true>(const Vector& pos,
    const Size index,
    const Float radius,
    Array<NeighborRecord>& neighs) const;

template Size HashMapFinder::find<false>(const Vector& pos,
    const Size index,
    const Float radius,
    Array<NeighborRecord>& neighs) const;

NAMESPACE_SPH_END
