#pragma once

/// \file LookupMap.h
/// \brief Three-dimensional grid containing indices
/// \author Pavel Sevecek (seevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "geometry/Box.h"
#include "geometry/Vector.h"
#include "objects/containers/Array.h"

NAMESPACE_SPH_BEGIN

class LookupMap : public Noncopyable {
private:
    /// 3d grid of particles (stored as indices)
    Array<Array<Size>> storage;

    Box tightBox;
    Box boundingBox;

    Size dimensionSize;

public:
    LookupMap() = default;

    LookupMap(const Size n)
        : storage(pow<3>(n))
        , dimensionSize(n) {}

    void update(ArrayView<const Vector> points) {
        tightBox = Box();
        for (const Vector& v : points) {
            tightBox.extend(v);
        }
        boundingBox = this->extendBox(tightBox);
        for (auto& a : storage) {
            a.clear();
        }
        // put particles into voxels
        for (Size i = 0; i < points.size(); ++i) {
            Indices idxs = this->map(points[i]);
            (*this)(idxs).push(i);
        }
    }

    INLINE bool empty() const {
        return storage.empty();
    }

    INLINE Vector clamp(const Vector& pos) const {
        return tightBox.clamp(pos);
    }

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

    INLINE Vector getVoxelSize() const {
        return boundingBox.size() / dimensionSize;
    }

    INLINE Size getDimensionSize() const {
        return dimensionSize;
    }

    INLINE Indices map(const Vector& v) const {
        ASSERT(boundingBox.size()[X] > 0._f && boundingBox.size()[Y] > 0._f && boundingBox.size()[Z] > 0._f);
        ASSERT(dimensionSize >= 2);
        Vector idxs = (v - boundingBox.lower()) / (boundingBox.size()) * dimensionSize;
        ASSERT(idxs[X] >= 0 && idxs[Y] >= 0 && idxs[Z] >= 0);
        ASSERT(idxs[X] < dimensionSize && idxs[Y] < dimensionSize && idxs[Z] < dimensionSize);
        return Indices(idxs);
    }

private:
    // extends the bounding box by EPS in each dimension so we don't have to deal with particles lying on the
    // boundary.
    Box extendBox(const Box& box) const {
        const Vector extension = max(EPS * box.size(), Vector(EPS));
        Box extendedBox = box;
        extendedBox.extend(box.upper() + extension);
        extendedBox.extend(box.lower() - extension);
        return extendedBox;
    }
};

NAMESPACE_SPH_END
