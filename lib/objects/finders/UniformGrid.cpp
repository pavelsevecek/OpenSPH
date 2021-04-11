#include "objects/finders/UniformGrid.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

UniformGridFinder::UniformGridFinder(const Float relativeCellCnt)
    : relativeCellCnt(relativeCellCnt) {
    Indices::init();
}

UniformGridFinder::~UniformGridFinder() = default;

void UniformGridFinder::buildImpl(IScheduler& UNUSED(scheduler), ArrayView<const Vector> points) {
    PROFILE_SCOPE("VoxelFinder::buildImpl");
    // number of voxels, free parameter
    const Size lutSize = Size(relativeCellCnt * root<3>(points.size())) + 1;
    if (lut.empty() || lutSize != lut.getDimensionSize()) {
        // build lookup map if not yet build or we have a significantly different number of points
        lut = LookupMap(lutSize);
    }
    if (SPH_LIKELY(!points.empty())) {
        lut.update(points);
    }
}

template <bool FindAll>
Size UniformGridFinder::find(const Vector& pos,
    const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours) const {
    const Vector refPosition = lut.clamp(pos);
    Indices lower = lut.map(refPosition);
    Indices upper = lower;
    Box voxel = lut.voxel(lower);
    const Vector size = lut.getVoxelSize();
    Vector diffUpper = voxel.upper() - pos;
    Vector diffLower = pos - voxel.lower();

    SPH_ASSERT(lut.getDimensionSize() > 0);
    const int upperLimit = lut.getDimensionSize() - 1;
    while (upper[X] < upperLimit && diffUpper[X] < radius) {
        diffUpper[X] += size[X];
        upper[X]++;
    }
    while (lower[X] > 0 && diffLower[X] < radius) {
        diffLower[X] += size[X];
        lower[X]--;
    }
    while (upper[Y] < upperLimit && diffUpper[Y] < radius) {
        diffUpper[Y] += size[Y];
        upper[Y]++;
    }
    while (lower[Y] > 0 && diffLower[Y] < radius) {
        diffLower[Y] += size[Y];
        lower[Y]--;
    }
    while (upper[Z] < upperLimit && diffUpper[Z] < radius) {
        diffUpper[Z] += size[Z];
        upper[Z]++;
    }
    while (lower[Z] > 0 && diffLower[Z] < radius) {
        diffLower[Z] += size[Z];
        lower[Z]--;
    }

    for (int x = lower[X]; x <= upper[X]; ++x) {
        for (int y = lower[Y]; y <= upper[Y]; ++y) {
            for (int z = lower[Z]; z <= upper[Z]; ++z) {
                for (Size i : lut(Indices(x, y, z))) {
                    const Float distSqr = getSqrLength(values[i] - pos);
                    if (distSqr < sqr(radius) && (FindAll || rank[i] < rank[index])) {
                        neighbours.emplaceBack(NeighbourRecord{ i, distSqr });
                    }
                }
            }
        }
    }
    return neighbours.size();
}

NAMESPACE_SPH_END
