#pragma once

/// \file LookupMap.h
/// \brief Three-dimensional grid containing indices
/// \author Pavel Sevecek (seevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/Array.h"
#include "objects/geometry/Box.h"
#include "objects/geometry/Vector.h"

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
        boundingBox = this->extendBox(tightBox, 1.e-6_f);
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
        ASSERT(dimensionSize >= 1);
        Vector idxs = (v - boundingBox.lower()) / (boundingBox.size()) * dimensionSize;
        ASSERT(idxs[X] >= 0 && idxs[Y] >= 0 && idxs[Z] >= 0);
        ASSERT(idxs[X] < dimensionSize && idxs[Y] < dimensionSize && idxs[Z] < dimensionSize);
        return Indices(idxs);
    }

private:
    /// \brief Extends the bounding box by EPS in each dimension.
    ///
    /// This is needed to avoid particles lying on the boundary, causing issues when assigning them to voxels.
    /// Settings the actual epsilon for extension is a little bit tricky here: it has to scale with the size
    /// of the box, but at the same it has to handle cases where the whole bounding box is very far from the
    /// origin.
    Box extendBox(const Box& box, const Float eps) const {
        const Vector extension =
            max(eps * box.size(), eps * abs(box.lower()), eps * abs(box.upper()), Vector(eps));
        Box extendedBox = box;
        extendedBox.extend(box.upper() + extension);
        extendedBox.extend(box.lower() - extension);
        return extendedBox;
    }
};

NAMESPACE_SPH_END
