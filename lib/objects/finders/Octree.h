#pragma once

/// \file Octree.h
/// \brief Implementation of Octree algorithm for kNN queries.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "objects/containers/Array.h"
#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Box.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

constexpr Size MAX_CHILDREN_PER_LEAF = 1;

/*struct OctreeNode {
    StaticArray<AutoPtr<OctreeNode>, 8> children;
    Box voxel;
    Array<Size> points;
    bool isLeaf = false;

    void setPoints(Array<Size>&& pts) {
        points = std::move(pts);
        isLeaf = true;
    }

    void setPoints(const IndexSequence& sequence) {
        points.reserve(sequence.size());
        for (Size i : sequence) {
            points.push(i);
        }
        isLeaf = true;
    }
};

class Octree : public FinderTemplate<Octree> {
private:
    Box box;
    AutoPtr<OctreeNode> root;

protected:
    virtual void buildImpl(ArrayView<const Vector> points) override {
        PROFILE_SCOPE("Octree::buildImpl");
        box = Box();
        for (const Vector& p : points) {
            box.extend(p);
        }
        root = makeNode(box, points, IndexSequence(0, points.size()));
    }

    virtual void rebuildImpl(ArrayView<const Vector> points) override {
        buildImpl(points);
    }

public:
    Octree() = default;

    virtual Size findNeighbours(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override {
        PROFILE_SCOPE("Octree::findNeighbours");
        ASSERT(neighbours.empty())
        ASSERT(root);
        findNeighboursInNode(*root, index, radius, neighbours);
        return neighbours.size();
    }

    virtual Size findNeighbours(const Vector& UNUSED(position),
        const Float radius,
        Array<NeighbourRecord>& neighbours) const override {
        PROFILE_SCOPE("Octree::findNeighbours");
        neighbours.clear();
        ASSERT(root);
        findNeighboursInNode(*root, 0, radius, neighbours);
        NOT_IMPLEMENTED;
        return neighbours.size();
    }

    template <typename TFunctor>
    void enumerateChildren(TFunctor&& functor) const {
        enumerateChildrenNode(*root, std::forward<TFunctor>(functor));
    }

private:
    template <typename TFunctor>
    void enumerateChildrenNode(OctreeNode& node, TFunctor&& functor) const {
        functor(node);
        if (!node.isLeaf) {
            for (auto& child : node.children) {
                enumerateChildrenNode(*child, functor);
            }
        }
    }

    void findNeighboursInNode(OctreeNode& node,
        const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours) const {
        const Float radiusSqr = sqr(radius);
        if (node.isLeaf) {
            for (Size i : node.points) {
                const Float distSqr = getSqrLength(values[i] - values[index]);
                if (distSqr < radiusSqr && (this->findAll() || rankH[i] < rankH[index])) {
                    neighbours.push(NeighbourRecord{ i, distSqr });
                }
            }
        } else {
            const Vector& p = values[index];
            const Size code = this->getCode(p, node.voxel);
            Box voxel = node.children[code]->voxel;
            if (getSqrLength(voxel.lower() - p) < radiusSqr && getSqrLength(voxel.upper() - p) < radiusSqr) {
                // all points of the voxel are already the sphere, skip the checks and just add all points
                this->enumerateChildrenNode(node, [this, &p, refRank, &neighbours](OctreeNode& n) INL {
                    if (!n.isLeaf) {
                        return;
                    }
                    for (Size i : n.points) {
                        const Float distSqr = getSqrLength(values[i] - p);
                        if (rankH[i] < refRank) {
                            neighbours.push(NeighbourRecord{ i, distSqr });
                        }
                    }
                });
                return;
            }*/
/*Vector& l = voxel.lower();
            Vector& u = voxel.upper();
            const Vector r(radius);
            l += r;
            u -= r;
            if (voxel.contains(p)) {
                // sphere is entirely in this octant, no need to check the other octants
                this->findNeighboursInNode(*node.children[code], index, radius, neighbours, refRank);
            } else*/ /*{
                for (Size i = 0; i < node.children.size(); ++i) {
                    OctreeNode& child = *node.children[i];
                    const bool intersect = (i == code) ? true : sphereIntersectsBox(p, radius, child.voxel);
                    if (intersect) {
                        ASSERT(node.children[i]);
                        this->findNeighboursInNode(child, index, radius, neighbours);
                    }
                }
            }
        }
    }

    template <typename TIdxs>
    AutoPtr<OctreeNode> makeNode(const Box& voxel, ArrayView<const Vector> points, TIdxs&& idxs) {
        AutoPtr<OctreeNode> node = makeAuto<OctreeNode>();
        node->voxel = voxel;
        if (idxs.size() <= MAX_CHILDREN_PER_LEAF) {
            node->setPoints(std::forward<TIdxs>(idxs));
            ASSERT(node->isLeaf);
        } else {
            StaticArray<Array<Size>, 8> octants;
            for (Size i : idxs) {
                ASSERT(voxel.contains(points[i]));
                const Size idx = getCode(points[i], voxel);
                octants[idx].push(i);
            }
            this->reset(std::forward<TIdxs>(idxs)); // dealloc
            for (Size j = 0; j < node->children.size(); ++j) {
                node->children[j] = makeNode(this->getVoxel(j, voxel), points, std::move(octants[j]));
            }
            ASSERT(!node->isLeaf);
        }
        return node;
    }

    INLINE void reset(Array<Size>&& points) {
        Array<Size>().swap(points);
    }

    INLINE void reset(const IndexSequence&) {
        // do nothing
    }

    INLINE bool sphereIntersectsBox(const Vector& p, const Float radius, const Box& voxel) const {
        Float dmin = 0._f;
        for (Size i = 0; i < 3; ++i) {
            if (p[i] < voxel.lower()[i]) {
                dmin += sqr(p[i] - voxel.lower()[i]);
            } else if (p[i] > voxel.upper()[i]) {
                dmin += sqr(p[i] - voxel.upper()[i]);
            }
        }
        return dmin <= sqr(radius);
    }

    Box getVoxel(const Size code, const Box& voxel) const {
        Box b = voxel;
        const Vector size = 0.5_f * voxel.size();
        if (code & 0x01) {
            b.lower()[X] += size[X];
        } else {
            b.upper()[X] -= size[X];
        }
        if (code & 0x02) {
            b.lower()[Y] += size[Y];
        } else {
            b.upper()[Y] -= size[Y];
        }
        if (code & 0x04) {
            b.lower()[Z] += size[Z];
        } else {
            b.upper()[Z] -= size[Z];
        }
        return b;
    }

    INLINE Size getCode(const Vector& p, const Box& voxel) const {
        const Vector& center = voxel.center();
        Size code = 0;
        if (p[X] > center[X]) {
            code |= 0x01;
        }
        if (p[Y] > center[Y]) {
            code |= 0x02;
        }
        if (p[Z] > center[Z]) {
            code |= 0x04;
        }
        ASSERT(code < 8);
        return code;
    }
};*/

NAMESPACE_SPH_END
