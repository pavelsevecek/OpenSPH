#pragma once

#include "geometry/Box.h"
#include "geometry/Multipole.h"
#include "objects/finders/AbstractFinder.h"

NAMESPACE_SPH_BEGIN

/// https://www.cs.umd.edu/~mount/Papers/cgc99-smpack.pdf
class KdTree : public Abstract::Finder {
private:
    Size leafSize;
    Box entireBox;

    Array<Size> idxs;

    /// Here X, Y, Z must be 0, 1, 2
    enum class NodeType { X, Y, Z, LEAF };

    /// \todo possibly make KdTree templated to allow modifying Node

    struct Node {

        NodeType type;

        Multipole<4> M;

        Node(const NodeType& type)
            : type(type) {}

        INLINE bool isLeaf() const {
            return type == NodeType::LEAF;
        }
    };

    struct InnerNode : public Node {
        float splitPosition;

        Node* left = nullptr;
        Node* right = nullptr;

        InnerNode(const NodeType& type)
            : Node(type) {}
    };

    struct LeafNode : public Node {
        Size from;
        Size to;

        Size padding[3];

        LeafNode(const NodeType& type)
            : Node(type) {}
    };

    struct PlaceHolder : public Node {
        Size padding[5];

        PlaceHolder(const NodeType& type)
            : Node(type) {}

        operator InnerNode&() {
            ASSERT(!this->isLeaf());
            return reinterpret_cast<InnerNode&>(*this);
        }

        operator LeafNode&() {
            ASSERT(this->isLeaf());
            return reinterpret_cast<LeafNode&>(*this);
        }
    };

    static_assert(sizeof(PlaceHolder) == sizeof(InnerNode), "Sizes of nodes must match");
    static_assert(sizeof(LeafNode) == sizeof(InnerNode), "Sizes of nodes must match");

    Array<PlaceHolder> nodes;

    static constexpr Size ROOT_NODE = -1;

protected:
    virtual void buildImpl(ArrayView<const Vector> points) override {
        entireBox = Box();
        idxs.clear();
        for (const auto i : iterateWithIndex(points)) {
            entireBox.extend(i.value());
            idxs.push(i.index());
        }

        this->buildTree(ROOT_NODE, 0, points.size(), entireBox);
    }

    virtual void rebuildImpl(ArrayView<const Vector> points) override {
        this->buildImpl(points);
    }

    void buildTree(const Size parent, const Size from, const Size to, const Box& box) {
        const Size splitIdx = argMax(box.size());

        bool isLeaf = false;
        if (to - from > leafSize) {
            /*for (Size dim = 0; dim < 3; ++dim) {
            }*/
        } else {
            isLeaf = true;
        }

        Box box1, box2;

        if (isLeaf) {
            this->addLeaf(parent, from, to);
        } else {
            // split around center of the box
            Float splitPosition = box.center()[splitIdx];

            Size n1 = from, n2 = to - 1;
            for (; n1 < to && values[n1][splitIdx] <= splitPosition; ++n1)
                ;
            // if from is 0, we compare unsigned value against 0, but it should never happen anyway (?)
            for (; n2 >= from && values[n2][splitIdx] >= splitPosition; --n2)
                ;
            ASSERT(n1 <= n2);
            if (n1 < n2) {
                std::swap(idxs[n1], idxs[n2]);
            }

            if (n1 == from) {
                Size idx = from;
                splitPosition = values[idxs[from]][splitIdx];
                for (Size i = from + 1; i < to; ++i) {
                    const Float x1 = values[idxs[i]][splitIdx];
                    if (x1 < splitPosition) {
                        idx = i;
                        splitPosition = x1;
                    }
                }
                std::swap(idxs[from], idxs[idx]);
                n1++;
            } else if (n1 == to) {
                Size idx = from;
                splitPosition = values[idxs[from]][splitIdx];
                for (Size i = from + 1; i < to; ++i) {
                    const Float x2 = values[idxs[i]][splitIdx];
                    if (x2 > splitPosition) {
                        idx = i;
                        splitPosition = x2;
                    }
                }
                std::swap(idxs[to - 1], idxs[idx]);
                n2--;
            }

            tie(box1, box2) = box.split(splitIdx, splitPosition);


            this->buildTree(nodes.size(), from, n1, box1);
            this->buildTree(nodes.size(), n1, to, box2);
        }
    }

    void addLeaf(const Size parent, const Size from, const Size to) {
        nodes.emplaceBack(NodeType::LEAF);
        const Size index = nodes.size() - 1;
        LeafNode& node = nodes[index];

        node.from = from;
        node.to = to;

        InnerNode& parentNode = nodes[parent];
        if (index == parent - 2 || parent != ROOT_NODE) {
            // right child
            parentNode.right = &node;
        } else {
            parentNode.left = &node;
        }
    }

    void addInner(const Size parent, const Float splitPosition, const Size splitIdx) {
        nodes.emplaceBack(NodeType(splitIdx));
        const Size index = nodes.size() - 1;
        InnerNode& node = nodes[index];
        node.splitPosition = splitPosition;

        InnerNode& parentNode = nodes[parent];
        if (index == parent - 2 || parent != ROOT_NODE) {
            // right child
            parentNode.right = &node;
        } else {
            parentNode.left = &node;
        }
    }

public:
    KdTree(const Size leafSize = 20)
        : leafSize(leafSize) {}

    virtual Size findNeighbours(const Size index,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override {
        MARK_USED(index);
        MARK_USED(radius);
        MARK_USED(neighbours);
        MARK_USED(flags);
        MARK_USED(error);
        return 0;
    }

    /// Finds all points within given radius from given position. The position may not correspond to any
    /// point.
    virtual Size findNeighbours(const Vector& position,
        const Float radius,
        Array<NeighbourRecord>& neighbours,
        Flags<FinderFlags> flags = EMPTY_FLAGS,
        const Float error = 0._f) const override {
        MARK_USED(position);
        MARK_USED(radius);
        MARK_USED(neighbours);
        MARK_USED(flags);
        MARK_USED(error);
        return 0;
    }
};

NAMESPACE_SPH_END
