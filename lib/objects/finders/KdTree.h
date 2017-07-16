#pragma once

#include "geometry/Box.h"
#include "geometry/Multipole.h"
#include "objects/finders/AbstractFinder.h"

NAMESPACE_SPH_BEGIN

/// Base class for nodes of K-d tree
struct KdNode {
    /// Here X, Y, Z must be 0, 1, 2
    enum class Type { X, Y, Z, LEAF };
    Type type;

    /// Bounding box of particles in the node
    Box box;

    /// Center of mass of contained particles
    Vector com;

    /// Gravitational moments with a respect to the center of mass, using expansion to octupole order.
    MultipoleExpansion<3> moments;

    KdNode(const Type& type)
        : type(type) {}

    INLINE bool isLeaf() const {
        return type == Type::LEAF;
    }
};

/// Inner node of K-d tree
struct InnerNode : public KdNode {
    /// Position where the selected dimension is split
    float splitPosition;

    /// Index of left child node
    Size left;

    /// Index of right child node
    Size right;
};

/// Leaf (bucket) node of K-d tree
struct LeafNode : public KdNode {
    /// First index of particlse belonging to the leaf
    Size from;

    /// One-past-last index of particles belonging to the leaf
    Size to;

    /// Unused, used so that LeafNode and InnerNode have the same size
    Size padding;

    /// Returns the number of points in the leaf. Can be zero.
    INLINE Size size() const {
        return to - from;
    }
};

/// Index iterator with given mapping (index permutation), returns value mapping[index] when dereferenced,
class LeafIndexIterator : public IndexIterator {
private:
    ArrayView<const Size> mapping;

public:
    INLINE LeafIndexIterator(const Size idx, ArrayView<const Size> mapping)
        : IndexIterator(idx)
        , mapping(mapping) {}

    INLINE Size operator*() const {
        return mapping[idx];
    }
};

/// Helper index sequence to iterate over particle indices of a leaf node.
class LeafIndexSequence : public IndexSequence {
private:
    ArrayView<const Size> mapping;

public:
    INLINE LeafIndexSequence(const Size from, const Size to, ArrayView<const Size> mapping)
        : IndexSequence(from, to)
        , mapping(mapping) {
        ASSERT(to <= mapping.size());
    }

    INLINE LeafIndexIterator begin() const {
        return LeafIndexIterator(from, mapping);
    }

    INLINE LeafIndexIterator end() const {
        return LeafIndexIterator(to, mapping);
    }
};


/// https://www.cs.umd.edu/~mount/Papers/cgc99-smpack.pdf
class KdTree : public Abstract::Finder {
private:
    Size leafSize;
    Box entireBox;

    /// \todo some variables are not necessary to store: left child (always parent+1), box of inner nodes,

    Array<Size> idxs;

    /// \todo possibly make KdTree templated to allow modifying Node

    struct PlaceHolderNode : public KdNode {
        Size padding[3];

        PlaceHolderNode(const KdNode::Type& type)
            : KdNode(type) {}
    };

    Array<PlaceHolderNode> nodes;

    static constexpr Size ROOT_PARENT_NODE = -1;

public:
    KdTree(const Size leafSize = 20)
        : leafSize(leafSize) {
        ASSERT(leafSize >= 1);
    }

    virtual Size findNeighbours(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override;

    virtual Size findNeighbours(const Vector& position,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override;

    enum class Direction {
        TOP_DOWN,  ///< From root to leaves
        BOTTOM_UP, ///< From leaves to root
    };

    /// \brief Calls a functor for every node in specified direction.
    ///
    /// The functor is called with the node as a parameter. For top-down direction, functor may return false
    /// to skip all children nodes from processing, otherwise the iteration proceedes through the tree into
    /// leaf nodes.
    /// \param functor Functor executed for every node
    /// \param nodeIdx Index of the first processed node; use 0 for root node
    template <Direction Dir, typename TFunctor>
    void iterate(const TFunctor& functor, const Size nodeIdx = 0) {
        KdNode& node = nodes[nodeIdx];
        if (Dir == Direction::TOP_DOWN) {
            if (node.isLeaf()) {
                functor(node, nullptr, nullptr);
            } else {
                InnerNode& inner = (InnerNode&)node;
                if (!functor(inner, &nodes[inner.left], &nodes[inner.right])) {
                    return;
                }
            }
        }
        if (!node.isLeaf()) {
            const InnerNode& inner = (const InnerNode&)node;
            this->iterate<Dir>(functor, inner.left);
            this->iterate<Dir>(functor, inner.right);
        }
        if (Dir == Direction::BOTTOM_UP) {
            if (node.isLeaf()) {
                functor(node, nullptr, nullptr);
            } else {
                InnerNode& inner = (InnerNode&)node;
                functor(inner, &nodes[inner.left], &nodes[inner.right]);
            }
        }
    }

    /// Returns the node with given index
    INLINE const KdNode& getNode(const Size nodeIdx) const {
        return nodes[nodeIdx];
    }

    /// Returns the sequence of particles indices belonging to given leaf.
    INLINE LeafIndexSequence getLeafIndices(const LeafNode& leaf) const {
        return LeafIndexSequence(leaf.from, leaf.to, idxs);
    }

    /// Performs some checks of KdTree consistency, returns true if everything is OK
    bool sanityCheck() const;

protected:
    virtual void buildImpl(ArrayView<const Vector> points) override;

    virtual void rebuildImpl(ArrayView<const Vector> points) override;

private:
    void init();

    Size findNeighboursImpl(const Vector& position,
        const Float radius,
        const Size refRank,
        Array<NeighbourRecord>& neighbours) const;

    void buildTree(const Size parent, const Size from, const Size to, const Box& box, const Size slidingCnt);

    void addLeaf(const Size parent, const Size from, const Size to);

    void addInner(const Size parent, const Float splitPosition, const Size splitIdx);

    bool isSingular(const Size from, const Size to, const Size splitIdx) const;

    bool checkBoxes(const Size from, const Size to, const Size mid, const Box& box1, const Box& box2) const;
};

NAMESPACE_SPH_END
