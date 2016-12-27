#include "objects/finders/Voxel.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

VoxelFinder::VoxelFinder() = default;

VoxelFinder::~VoxelFinder() = default;

void VoxelFinder::buildImpl(ArrayView<Vector> values) {
    // number of voxels, free parameter
    const int lutSize = Math::root<3>(values.size()) + 1;
    // find bounding box
    Box boundingBox;
    for (Vector& v : values) {
        boundingBox.extend(v);
    }
    lut = LookupMap(lutSize, boundingBox);
    // put particles into voxels
    for (int i = 0; i < values.size(); ++i) {
        Indices idxs = lut.map(values[i]);
        lut(idxs).push(i);
    }
}

int VoxelFinder::findNeighbours(const int index,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> flags,
    const Float UNUSED(error)) const {
    neighbours.clear();
    const int refRank =
        (flags.has(FinderFlags::FIND_ONLY_SMALLER_H)) ? this->rankH[index] : this->values.size();

    Indices lower = lut.map(values[index]);
    Indices upper = lower;
    Box voxel = lut.voxel(lower);
    const Vector size = lut.getVoxelSize();
    Vector diffUpper = voxel.upper() - values[index];
    Vector diffLower = values[index] - voxel.lower();
    {
        PROFILE_SCOPE("Finding box");

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
    }
    {
        PROFILE_SCOPE("Filling neighs")
        for (int x = lower[X]; x <= upper[X]; ++x) {
            for (int y = lower[Y]; y <= upper[Y]; ++y) {
                for (int z = lower[Z]; z <= upper[Z]; ++z) {
                    for (int i : lut(Indices(x, y, z))) {
                        const Float distSqr = getSqrLength(values[i] - values[index]);
                        if (rankH[i] < refRank && distSqr < Math::sqr(radius)) {
                            neighbours.push(NeighbourRecord{ i, distSqr });
                        }
                    }
                }
            }
        }
    }
    return neighbours.size();
}

NAMESPACE_SPH_END
