#pragma once

/// \file KdTree.h
/// \brief K-d tree for efficient search of neighbouring particles.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz))
/// \date 2016-2018

#include "objects/finders/NeighbourFinder.h"
#include "objects/geometry/Box.h"
#include "objects/utility/IteratorAdapters.h"
#include "objects/wrappers/Finally.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/Outcome.h"
#include "thread/ThreadLocal.h"
#include <set>
#include <shared_mutex>

NAMESPACE_SPH_BEGIN

/// \brief Base class for nodes of K-d tree.
///
/// Can be derived to include additional user data for each node.
struct KdNode : public Noncopyable {
    /// Here X, Y, Z must be 0, 1, 2
    enum class Type : Size { X, Y, Z, LEAF };
    Type type;

    /// Bounding box of particles in the node
    Box box;

    KdNode(const Type& type)
        : type(type) {}

    INLINE bool isLeaf() const {
        return type == Type::LEAF;
    }
};

/// \brief Inner node of K-d tree
template <typename TBase>
struct InnerNode : public TBase {
    /// Position where the selected dimension is split
    float splitPosition;

    /// Index of left child node
    Size left;

    /// Index of right child node
    Size right;

    InnerNode()
        : TBase(KdNode::Type(-1)) {}

    InnerNode(const KdNode::Type& type)
        : TBase(type) {}
};

/// \brief Leaf (bucket) node of K-d tree
template <typename TBase>
struct LeafNode : public TBase {
    /// First index of particlse belonging to the leaf
    Size from;

    /// One-past-last index of particles belonging to the leaf
    Size to;

    /// Unused, used so that LeafNode and InnerNode have the same size
    Size padding;

    LeafNode(const KdNode::Type& type)
        : TBase(type) {}

    /// Returns the number of points in the leaf. Can be zero.
    INLINE Size size() const {
        return to - from;
    }
};

static_assert(sizeof(Size) == sizeof(float), "Sizes must match to keep this layout");

/// \brief Index iterator with given mapping (index permutation).
///
/// Returns value mapping[index] when dereferenced,
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

/// \brief Helper index sequence to iterate over particle indices of a leaf node.
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

struct EuclideanMetric {
    INLINE Float operator()(const Vector& v) const {
        return getSqrLength(v);
    }
};

enum class KdChild;

/// \brief K-d tree, used for hierarchical clustering of particles and accelerated Kn queries.
///
/// Allows storing arbitrary data at each node of the tree.
///
/// https://www.cs.umd.edu/~mount/Papers/cgc99-smpack.pdf
/// \tparam TNode Nodes of the tree, should always derive from KdNode and should be POD structs.
/// \tparam TMetric Functor returning the squared distance of two vectors.
template <typename TNode, typename TMetric = EuclideanMetric>
class KdTree : public FinderTemplate<KdTree<TNode, TMetric>> {
private:
    const Size leafSize;

    Box entireBox;

    Array<Size> idxs;

    /// Holds all nodes, either \ref InnerNode or \ref LeafNode (depending on the value of \ref type).
    Array<InnerNode<TNode>> nodes;
    std::atomic_int nodeCounter;
    std::shared_timed_mutex nodesMutex;

    static constexpr Size ROOT_PARENT_NODE = -1;

public:
    explicit KdTree(const Size leafSize = 20)
        : leafSize(leafSize) {
        ASSERT(leafSize >= 1);
    }

    template <bool FindAll>
    Size find(const Vector& pos, const Size index, const Float radius, Array<NeighbourRecord>& neighs) const;

    /// \brief Returns the node with given index
    INLINE TNode& getNode(const Size nodeIdx) {
        return nodes[nodeIdx];
    }

    /// \brief Returns the node with given index
    INLINE const TNode& getNode(const Size nodeIdx) const {
        return nodes[nodeIdx];
    }

    /// \brief Returns the number of nodes in the tree
    INLINE Size getNodeCnt() const {
        return nodes.size();
    }

    /// \brief Returns the sequence of particles indices belonging to given leaf.
    INLINE LeafIndexSequence getLeafIndices(const LeafNode<TNode>& leaf) const {
        return LeafIndexSequence(leaf.from, leaf.to, idxs);
    }

    /// \brief Performs some checks of KdTree consistency, returns SUCCESS if everything is OK
    Outcome sanityCheck() const;

protected:
    virtual void buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) override;

private:
    void init();

    void buildTree(IScheduler& scheduler,
        const Size parent,
        const KdChild child,
        const Size from,
        const Size to,
        const Box& box,
        const Size slidingCnt);

    void addLeaf(const Size parent, const KdChild child, const Size from, const Size to);

    Size addInner(const Size parent, const KdChild child, const Float splitPosition, const Size splitIdx);

    bool isSingular(const Size from, const Size to, const Size splitIdx) const;

    bool checkBoxes(const Size from, const Size to, const Size mid, const Box& box1, const Box& box2) const;
};


enum class IterateDirection {
    TOP_DOWN,  ///< From root to leaves
    BOTTOM_UP, ///< From leaves to root
};

/// \brief Calls a functor for every node of a K-d tree tree in specified direction.
///
/// The functor is called with the node as a parameter. For top-down direction, functor may return false
/// to skip all children nodes from processing, otherwise the iteration proceedes through the tree into
/// leaf nodes.
/// \param tree KdTree to iterate.
/// \param scheduler Scheduler used for sequential or parallelized task execution
/// \param functor Functor executed for every node
/// \param nodeIdx Index of the first processed node; use 0 for root node
template <IterateDirection Dir, typename TNode, typename TMetric, typename TFunctor>
void iterateTree(KdTree<TNode, TMetric>& tree,
    IScheduler& scheduler,
    const TFunctor& functor,
    const Size nodeIdx = 0,
    const Size depth = 0);

/// \copydoc iterateTree
template <IterateDirection Dir, typename TNode, typename TMetric, typename TFunctor>
void iterateTree(const KdTree<TNode, TMetric>& tree,
    IScheduler& scheduler,
    const TFunctor& functor,
    const Size nodeIdx = 0,
    const Size depth = 0);

NAMESPACE_SPH_END

#include "objects/finders/KdTree.inl.h"
