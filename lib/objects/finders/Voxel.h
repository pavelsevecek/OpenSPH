#pragma once

/// Simple algorithm for finding nearest neighbours using spatial partitioning of particles
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Box.h"
#include "objects/finders/AbstractFinder.h"

NAMESPACE_SPH_BEGIN

class VoxelFinder : public Abstract::Finder {
protected:
    class LookupMap : public Noncopyable {
    private:
        Array<Array<int>> storage;
        Box boundingBox;
        int dimensionSize;

    public:
        LookupMap() = default;

        LookupMap(const int n, const Box& box)
            : storage(Math::pow<3>(n))
            , boundingBox(box)
            , dimensionSize(n) {}

        LookupMap& operator=(LookupMap&& other) {
            storage = std::move(other.storage);
            dimensionSize = other.dimensionSize;
            boundingBox = other.boundingBox;
            return *this;
        }

        INLINE const Array<int>& operator()(const int x, const int y, const int z) const {
            const int idx = x * Math::sqr(dimensionSize) + y * dimensionSize + z;
            ASSERT(unsigned(idx) < unsigned(storage.size()));
            return storage[idx];
        }

        INLINE Array<int>& operator()(const int x, const int y, const int z) {
            const int idx = x * Math::sqr(dimensionSize) + y * dimensionSize + z;
            ASSERT(unsigned(idx) < unsigned(storage.size()));
            return storage[idx];
        }

        INLINE Box voxel(const int x, const int y, const int z) const {
            Vector lower(boundingBox.lower() + boundingBox.size() * Vector(x, y, z) / dimensionSize);
            Vector upper(
                boundingBox.lower() + boundingBox.size() * Vector(x + 1, y + 1, z + 1) / dimensionSize);
            return Box(lower, upper);
        }

        INLINE Vector getVoxelSize() const { return boundingBox.size() / dimensionSize; }

        INLINE int getDimensionSize() const { return dimensionSize; }

        INLINE StaticArray<int, 3> map(const Vector& v) const {
            Vector idxs = (v - boundingBox.lower()) / (boundingBox.size() + Vector(EPS)) * dimensionSize;
            ASSERT(idxs[X] < dimensionSize && idxs[Y] < dimensionSize && idxs[Z] < dimensionSize);
            return makeStatic(int(idxs[X]), int(idxs[Y]), int(idxs[Z]));
        }
    };

    LookupMap lut;

    virtual void buildImpl(ArrayView<Vector> values) override {
        const int lutSize = Math::root<3>(values.size());
        // find bounding box
        Box boundingBox;
        for (Vector& v : values) {
            boundingBox.extend(v);
        }
        lut = LookupMap(lutSize, boundingBox);
        // put particles into voxels
        for (int i = 0; i < values.size(); ++i) {
            int x, y, z;
            tieToStatic(x, y, z) = lut.map(values[i]);
            lut(x, y, z).push(i);
        }
    }

public:
    VoxelFinder() = default;

    virtual int findNeighbours(const int index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float UNUSED(error) = 0._f) const override {
        neighbours.clear();
        const int refRank =
            (flags.has(FinderFlags::FIND_ONLY_SMALLER_H)) ? this->rankH[index] : this->values.size();

        Indices lower, upper;
        tieToStatic(lower[X], lower[Y], lower[Z]) = lut.map(values[index]);
        upper = lower;
        Box voxel = lut.voxel(lower[X], lower[Y], lower[Z]);
        const Vector size = lut.getVoxelSize();
        for (int dim = 0; dim < 3; ++dim) {
            while (
                upper[dim] < lut.getDimensionSize() - 1 && voxel.upper()[dim] - values[index][dim] < radius) {
                voxel.upper()[dim] += size[dim];
                upper[dim]++;
            }
            while (lower[dim] > 0 && values[index][dim] - voxel.lower()[dim] < radius) {
                voxel.lower()[dim] -= size[dim];
                lower[dim]--;
            }
        }
        for (int x = lower[X]; x <= upper[X]; ++x) {
            for (int y = lower[Y]; y <= upper[Y]; ++y) {
                for (int z = lower[Z]; z <= upper[Z]; ++z) {
                    for (int i : lut(x, y, z)) {
                        const Float distSqr = getSqrLength(values[i] - values[index]);
                        if (rankH[i] < refRank && distSqr < Math::sqr(radius)) {
                            neighbours.push(NeighbourRecord{ i, distSqr });
                        }
                    }
                }
            }
        }
        return neighbours.size();
    }


    /// Updates the structure when the position change.
    virtual void rebuild() {}
};

NAMESPACE_SPH_END
