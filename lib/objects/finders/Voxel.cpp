#include "objects/finders/Voxel.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

VoxelFinder::VoxelFinder() {
    Indices::init();
}

VoxelFinder::~VoxelFinder() = default;

void VoxelFinder::buildImpl(ArrayView<const Vector> points) {
    // number of voxels, free parameter
    const Size lutSize = root<3>(points.size()) + 1;
    // find bounding box
    Box boundingBox;
    for (const Vector& v : points) {
        boundingBox.extend(v);
    }
    if (lut.empty()) {
        lut = LookupMap(lutSize, boundingBox);
    } else {
        lut.update(boundingBox);
    }
    // put particles into voxels
    for (Size i = 0; i < points.size(); ++i) {
        Indices idxs = lut.map(points[i]);
        lut(idxs).push(i);
    }
}

void VoxelFinder::rebuildImpl(ArrayView<const Vector> points) {
    buildImpl(points);
}

Size VoxelFinder::findNeighbours(const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> flags,
    const Float UNUSED(error)) const {
    PROFILE_SCOPE("VoxelFinder::findNeighbours");
    neighbours.clear();
    const Size refRank =
        (flags.has(FinderFlags::FIND_ONLY_SMALLER_H)) ? this->rankH[index] : this->values.size();

    Indices lower = lut.map(values[index]);
    Indices upper = lower;
    Box voxel = lut.voxel(lower);
    const Vector size = lut.getVoxelSize();
    Vector diffUpper = voxel.upper() - values[index];
    Vector diffLower = values[index] - voxel.lower();

    ASSERT(lut.getDimensionSize() > 0);
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
                    const Float distSqr = getSqrLength(values[i] - values[index]);
                    if (rankH[i] < refRank && distSqr < sqr(radius)) {
                        neighbours.emplaceBack(NeighbourRecord{ i, distSqr });
                    }
                }
            }
        }
    }
    return neighbours.size();
}

NAMESPACE_SPH_END
