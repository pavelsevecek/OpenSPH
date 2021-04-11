#pragma once

/// \file Grid.h
/// \brief Three-dimensional dynamically-allocated container
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "objects/containers/Array.h"
#include "objects/geometry/Indices.h"
#include "objects/wrappers/AutoPtr.h"
#include "objects/wrappers/Variant.h"

NAMESPACE_SPH_BEGIN

template <typename T>
class Grid {
private:
    Array<T, uint64_t> data;
    Indices dimensions = Indices(0);

public:
    Grid() = default;

    Grid(const Indices& dimensions, const T& value = T())
        : dimensions(dimensions) {
        data.resizeAndSet(this->voxelCount(), value);
    }

    INLINE T& operator[](const Indices& idxs) {
        return data[map(idxs)];
    }

    INLINE const T& operator[](const Indices& idxs) const {
        return data[map(idxs)];
    }

    INLINE Indices size() const {
        return dimensions;
    }

    INLINE bool empty() const {
        return data.empty();
    }

    INLINE uint64_t voxelCount() const {
        return dimensions[X] * dimensions[Y] * dimensions[Z];
    }

    Iterator<T, uint64_t> begin() {
        return data.begin();
    }

    Iterator<T, uint64_t> end() {
        return data.end();
    }

private:
    INLINE uint64_t map(const Indices idxs) const {
        SPH_ASSERT(idxs[X] >= 0 && idxs[X] < dimensions[X]);
        SPH_ASSERT(idxs[Y] >= 0 && idxs[Y] < dimensions[Y]);
        SPH_ASSERT(idxs[Z] >= 0 && idxs[Z] < dimensions[Z]);
        return idxs[X] * dimensions[Y] * dimensions[Z] + idxs[Y] * dimensions[Z] + idxs[Z];
    }
};

template <typename T>
class OctreeNode {
private:
    using Children = StaticArray<AutoPtr<OctreeNode>, 8>;

    Variant<Children, T> data;

public:
    T& create(const Indices& idxs, const Size dim, const T& value) {
        if (dim == 1) {
            // create leaf node
            data = value;
            return data;
        }
        Size code;
        Indices childIdxs;
        tieToTuple(code, childIdxs) = this->getCodeAndChildIdxs(idxs, dim);

        Children& children = data;
        if (!children[code]) {
            children[code] = makeAuto<OctreeNode>();
        }
        return children[code]->create(childIdxs, dim / 2, value);
    }

    struct Hint {
        OctreeNode* node;
        Size dim;
    };

    Variant<T*, Hint> find(const Indices& idxs, const Size dim) {
        SPH_ASSERT(idxs[X] >= 0 && idxs[X] < int(dim), idxs, dim);
        SPH_ASSERT(idxs[Y] >= 0 && idxs[Y] < int(dim), idxs, dim);
        SPH_ASSERT(idxs[Z] >= 0 && idxs[Z] < int(dim), idxs, dim);
        if (this->isLeaf()) {
            return std::addressof(data.template get<T>());
        } else {
            Size code;
            Indices childIdxs;
            tieToTuple(code, childIdxs) = this->getCodeAndChildIdxs(idxs, dim);

            Children& children = data;
            if (children[code]) {
                return children[code]->find(childIdxs, dim / 2);
            } else {
                return Hint{ this, dim };
            }
        }
    }

    Variant<const T*, Hint> find(const Indices& idxs, const Size dim) const {
        Variant<T*, Hint> value = const_cast<OctreeNode*>(this)->find(idxs, dim);
        if (value.template has<T*>()) {
            return static_cast<const T*>(value.template get<T*>());
        } else {
            return value.template get<Hint>();
        }
    }

    template <typename TFunctor>
    void iterate(const Indices& from, const Indices& to, const TFunctor& functor) {
        if (this->isLeaf()) {
            SPH_ASSERT(all(to - from == Indices(1)));
            functor(data.template get<T>(), from);
            return;
        }

        Children& children = data;
        for (Size code = 0; code < 8; ++code) {
            if (!children[code]) {
                continue;
            }
            const Indices size((to[X] - from[X]) / 2, (to[Y] - from[Y]) / 2, (to[Z] - from[Z]) / 2);
            Indices n1 = from, n2 = to;
            if (code & 0x01) {
                n1[X] += size[X];
            } else {
                n2[X] -= size[X];
            }
            if (code & 0x02) {
                n1[Y] += size[Y];
            } else {
                n2[Y] -= size[Y];
            }
            if (code & 0x04) {
                n1[Z] += size[Z];
            } else {
                n2[Z] -= size[Z];
            }
            children[code]->iterate(n1, n2, functor);
        }
    }

    bool isLeaf() const {
        return data.template has<T>();
    }

private:
    INLINE Tuple<Size, Indices> getCodeAndChildIdxs(const Indices& idxs, const Size dim) const {
        SPH_ASSERT(dim > 1);
        const int half = int(dim / 2);
        Indices child = idxs;
        Size code = 0;
        if (idxs[X] >= half) {
            code |= 0x01;
            child[X] -= half;
        }
        if (idxs[Y] >= half) {
            code |= 0x02;
            child[Y] -= half;
        }
        if (idxs[Z] >= half) {
            code |= 0x04;
            child[Z] -= half;
        }
        SPH_ASSERT(code < 8);
        return { code, child };
    }
};

template <typename T>
class SparseGrid {
private:
    Size dimensions = 0;
    T defaultValue;

    OctreeNode<T> root;

public:
    SparseGrid() = default;

    SparseGrid(const Size dimensions, const T& value = T())
        : dimensions(dimensions)
        , defaultValue(value) {
        SPH_ASSERT(isPower2(dimensions));
    }

    INLINE T& operator[](const Indices& idxs) {
        Variant<T*, typename OctreeNode<T>::Hint> ptrOrHint = root.find(idxs, dimensions);
        if (ptrOrHint.template has<T*>()) {
            return *ptrOrHint.template get<T*>();
        } else {
            typename OctreeNode<T>::Hint hint = ptrOrHint;
            return hint.node->create(idxs, hint.dim, defaultValue);
        }
    }

    INLINE const T& operator[](const Indices& idxs) const {
        Variant<const T*, typename OctreeNode<T>::Hint> ptrOrHint = root.find(idxs, dimensions);
        if (ptrOrHint.template has<const T*>()) {
            return *ptrOrHint.template get<const T*>();
        } else {
            return defaultValue;
        }
    }

    template <typename TFunctor>
    void iterate(const TFunctor& functor) {
        root.iterate(Indices(0), Indices(dimensions), functor);
    }

    INLINE Size size() const {
        return dimensions;
    }

    INLINE bool empty() const {
        return dimensions == 0;
    }

    INLINE uint64_t voxelCount() const {
        const uint64_t d = dimensions;
        return d * d * d;
    }
};

NAMESPACE_SPH_END
