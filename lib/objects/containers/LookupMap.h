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
        : storage(pow<3>(n))
        , boundingBox(box)
        , dimensionSize(n) {
        extendBox();
    }

    void update(const Box& box) {
        boundingBox = box;
        for (Array<Size>& ar : storage) {
            ar.clear();
        }
        extendBox();
    }

    INLINE bool empty() const { return storage.empty(); }

    LookupMap& operator=(LookupMap&& other) {
        storage = std::move(other.storage);
        dimensionSize = other.dimensionSize;
        boundingBox = other.boundingBox;
        return *this;
    }

    INLINE const Array<Size>& operator()(const Indices& idxs) const {
        const Size idx = idxs[X] * sqr(dimensionSize) + idxs[Y] * dimensionSize + idxs[Z];
        ASSERT(unsigned(idx) < unsigned(storage.size()));
        return storage[idx];
    }

    INLINE Array<Size>& operator()(const Indices& idxs) {
        const Size idx = idxs[X] * sqr(dimensionSize) + idxs[Y] * dimensionSize + idxs[Z];
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
        Vector idxs = (v - boundingBox.lower()) / (boundingBox.size()) * dimensionSize;
        ASSERT(idxs[X] >= 0 && idxs[Y] >= 0 && idxs[Z] >= 0);
        ASSERT(idxs[X] < dimensionSize && idxs[Y] < dimensionSize && idxs[Z] < dimensionSize);
        return Indices(idxs);
    }

private:
    // extends the bounding box by EPS in each dimension so we don't have to deal with particles lying on the boundary.
    void extendBox() {
        boundingBox.extend(boundingBox.upper() + Vector(EPS));
        boundingBox.extend(boundingBox.lower() - Vector(EPS));
    }
};

NAMESPACE_SPH_END
