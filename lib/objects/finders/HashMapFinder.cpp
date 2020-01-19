#include "objects/finders/HashMapFinder.h"
#include "objects/geometry/Sphere.h"
#include "sph/kernel/Kernel.h"
#include "system/Factory.h"

NAMESPACE_SPH_BEGIN

HashMapFinder::Cell::Cell() = default;

HashMapFinder::Cell::~Cell() = default;

HashMapFinder::HashMapFinder(const RunSettings& settings) {
    kernelRadius = Factory::getKernel<3>(settings).radius();
}

HashMapFinder::~HashMapFinder() = default;

void HashMapFinder::buildImpl(IScheduler& UNUSED(scheduler), ArrayView<const Vector> points) {
    map.clear();
    cellSize = 0._f;
    for (Size i = 0; i < points.size(); ++i) {
        cellSize = max(cellSize, kernelRadius * points[i][H]);
    }

    for (Size i = 0; i < points.size(); ++i) {
        const Indices idxs = Indices(points[i] / cellSize);
        Cell& cell = map[idxs];
        cell.points.push(i);
        cell.box.extend(points[i]);
    }
}

template <bool FindAll>
Size HashMapFinder::find(const Vector& pos,
    const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighs) const {
    ASSERT(neighs.empty());
    const Indices idxs0 = Indices(pos / cellSize);
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
                            neighs.emplaceBack(NeighbourRecord{ i, distSqr });
                        }
                    }
                }
            }
        }
    }
    return neighs.size();
}

NAMESPACE_SPH_END
