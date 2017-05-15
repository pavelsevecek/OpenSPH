#include "objects/finders/Voxel.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

VoxelFinder::VoxelFinder() {
    Indices::init();
}

VoxelFinder::~VoxelFinder() = default;

void VoxelFinder::buildImpl(ArrayView<const Vector> points) {
    PROFILE_SCOPE("VoxelFinder::buildImpl");
    if (lut.empty()) {
        // number of voxels, free parameter
        const Size lutSize = root<3>(points.size()) + 1;
        lut = LookupMap(lutSize);
    }
    lut.update(points);
}

void VoxelFinder::rebuildImpl(ArrayView<const Vector> points) {
    buildImpl(points);
}

Size VoxelFinder::findNeighbours(const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> flags,
    const Float UNUSED(error)) const {
		const Size refRank =
        (flags.has(FinderFlags::FIND_ONLY_SMALLER_H)) ? this->rankH[index] : this->values.size();
		this->findNeighboursImpl(values[index], refRank, radius, neighbours, flags, error);
	}
	
	Size VoxelFinder::findNeighbours(const Vector& position,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> flags,
    const Float UNUSED(error)) const {
		const Size refRank = this->values.size();
		this->findNeighboursImpl(position, refRank, radius, neighbours, flags, error);
	}
		
Size VoxelFinder::findNeighboursImpl(const Vector& position, const Size refRank, const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags,
        const Float error) const;
    neighbours.clear();

    Indices lower = lut.map(position);
    Indices upper = lower;
    Box voxel = lut.voxel(lower);
    const Vector size = lut.getVoxelSize();
    Vector diffUpper = voxel.upper() - position;
    Vector diffLower = position - voxel.lower();

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
                    const Float distSqr = getSqrLength(values[i] - position);
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
