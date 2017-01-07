#pragma once

#include "geometry/Box.h"
#include "geometry/Vector.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class LookupMap : public Noncopyable {
private:
    Array<Array<Size>> storage;
    Box boundingBox;
    Size dimensionSize;

public:
    LookupMap() = default;

    LookupMap(const Size n, const Box& box)
        : storage(Math::pow<3>(n))
        , boundingBox(box)
        , dimensionSize(n) {
        fixSingularBox();
    }

    void update(const Box& box) {
        boundingBox = box;
        for (Array<Size>& ar : storage) {
            ar.clear();
        }
        fixSingularBox();
    }

    bool empty() const { return storage.empty(); }

    LookupMap& operator=(LookupMap&& other) {
        storage = std::move(other.storage);
        dimensionSize = other.dimensionSize;
        boundingBox = other.boundingBox;
        return *this;
    }

    INLINE const Array<Size>& operator()(const Indices& idxs) const {
        const Size idx = idxs[X] * Math::sqr(dimensionSize) + idxs[Y] * dimensionSize + idxs[Z];
        ASSERT(unsigned(idx) < unsigned(storage.size()));
        return storage[idx];
    }

    INLINE Array<Size>& operator()(const Indices& idxs) {
        const Size idx = idxs[X] * Math::sqr(dimensionSize) + idxs[Y] * dimensionSize + idxs[Z];
        ASSERT(unsigned(idx) < unsigned(storage.size()));
        return storage[idx];
    }

    INLINE Box voxel(const Indices idxs) const {
        Vector lower(boundingBox.lower() + boundingBox.size() * Vector(idxs) / dimensionSize);
        Vector upper(boundingBox.lower() + boundingBox.size() * Vector(idxs + Indices(1)) / dimensionSize);
        return Box(lower, upper);
    }

    INLINE Vector getVoxelSize() const { return boundingBox.size() / dimensionSize; }

    INLINE int getDimensionSize() const { return dimensionSize; }

    INLINE Indices map(const Vector& v) const {
        ASSERT(boundingBox.size()[X] > 0._f && boundingBox.size()[Y] > 0._f && boundingBox.size()[Z] > 0._f);
        ASSERT(dimensionSize >= 2);
        Vector idxs = (v - boundingBox.lower()) / (boundingBox.size()) * (dimensionSize - 1);
        ASSERT(idxs[X] < dimensionSize && idxs[Y] < dimensionSize && idxs[Z] < dimensionSize);
        return Indices(idxs);
    }

private:
    void fixSingularBox() {
        for (uint dim = 0; dim < 3; ++dim) {
            if (boundingBox.size()[dim] == 0._f) {
                boundingBox.lower()[dim] -= EPS;
                boundingBox.upper()[dim] += EPS;
            }
        }
    }
};

NAMESPACE_SPH_END
