#include "objects/finders/KdTree.h"
#include "objects/utility/ArrayUtils.h"
#include "objects/wrappers/Function.h"
#include <set>

NAMESPACE_SPH_BEGIN

/// Object holding index of the node and squared distance of the bounding box
struct ProcessedNode {
    /// Index into the nodeStack array. We cannot use pointers because the array might get reallocated.
    Size idx;

    Vector sizeSqr;

    Float distanceSqr;
};

/// Cached stack to avoid reallocation
/// It is thread_local to allow using KdTree from multiple threads
thread_local Array<ProcessedNode> nodeStack;

void KdTree::buildImpl(ArrayView<const Vector> points) {
    static_assert(sizeof(PlaceHolderNode) == sizeof(InnerNode), "Sizes of nodes must match");
    static_assert(sizeof(LeafNode) == sizeof(InnerNode), "Sizes of nodes must match");

    this->init();

    for (const auto i : iterateWithIndex(points)) {
        entireBox.extend(i.value());
        idxs.push(i.index());
    }

    if (SPH_LIKELY(!points.empty())) {
        this->buildTree(ROOT_PARENT_NODE, 0, points.size(), entireBox, 0);
    }
}

void KdTree::rebuildImpl(ArrayView<const Vector> points) {
    /// \todo Could be potentially optimized
    this->buildImpl(points);
}

void KdTree::buildTree(const Size parent,
    const Size from,
    const Size to,
    const Box& box,
    const Size slidingCnt) {
    Box box1, box2;
    Vector boxSize = box.size();

    // split by the dimension of largest extent
    Size splitIdx = argMax(boxSize);

    bool slidingMidpoint = false;
    bool degeneratedBox = false;

    if (to - from <= leafSize) {
        // enough points to fit inside one leaf
        this->addLeaf(parent, from, to);
        return;
    } else {
        // check for singularity of dimensions
        for (Size dim = 0; dim < 3; ++dim) {
            if (this->isSingular(from, to, splitIdx)) {
                boxSize[splitIdx] = 0.f;
                // find new largest dimension
                splitIdx = argMax(boxSize);

                if (boxSize == Vector(0._f)) {
                    // too many overlapping points, just split until they fit within a leaf,
                    // the code can handle this case, but it smells with an error ...
                    ASSERT(false, "Too many overlapping points, something is probably wrong ...");
                    degeneratedBox = true;
                    break;
                }
            } else {
                break;
            }
        }

        // split around center of the box
        Float splitPosition = box.center()[splitIdx];
        std::make_signed_t<Size> n1 = from, n2 = to - 1; // use ints for easier for loop ending with 0

        if (slidingCnt <= 5 && !degeneratedBox) {
            for (;; std::swap(idxs[n1], idxs[n2])) {
                for (; n1 < int(to) && values[idxs[n1]][splitIdx] <= splitPosition; ++n1)
                    ;
                for (; n2 >= int(from) && values[idxs[n2]][splitIdx] >= splitPosition; --n2)
                    ;
                if (n1 >= n2) {
                    break;
                }
            }

            if (n1 == int(from)) {
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
                slidingMidpoint = true;
            } else if (n1 == int(to)) {
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
                slidingMidpoint = true;
            }

            tie(box1, box2) = box.split(splitIdx, splitPosition);
        } else {
            n1 = (from + to) >> 1;
            // do quick select to sort elements around the midpoint
            Iterator<Size> iter = idxs.begin();
            if (!degeneratedBox) {
                std::nth_element(iter + from, iter + n1, iter + to, [this, splitIdx](Size i1, Size i2) {
                    return values[i1][splitIdx] < values[i2][splitIdx];
                });
            }

            tie(box1, box2) = box.split(splitIdx, values[idxs[n1]][splitIdx]);
        }

        // sanity check
        ASSERT(this->checkBoxes(from, to, n1, box1, box2));

        // add inner node and set connect to the parenet
        this->addInner(parent, splitPosition, splitIdx);

        // recurse to left and right subtree
        const Size index = nodes.size() - 1;
        this->buildTree(index, from, n1, box1, slidingMidpoint ? slidingCnt + 1 : 0);
        this->buildTree(index, n1, to, box2, slidingMidpoint ? slidingCnt + 1 : 0);
    }
}

void KdTree::addLeaf(const Size parent, const Size from, const Size to) {
    nodes.emplaceBack(KdNode::Type::LEAF);
    const Size index = nodes.size() - 1;
    LeafNode& node = (LeafNode&)nodes[index];
    ASSERT(node.isLeaf());

#ifdef SPH_DEBUG
    node.from = node.to = -1;
#endif

    node.from = from;
    node.to = to;

    // find the bounding box of the leaf
    Box box;
    for (Size i = from; i < to; ++i) {
        box.extend(values[idxs[i]]);
    }
    node.box = box;

    if (parent == ROOT_PARENT_NODE) {
        return;
    }
    InnerNode& parentNode = (InnerNode&)nodes[parent];
    if (index == parent + 1) {
        // left child
        parentNode.left = index;
    } else {
        // right child
        parentNode.right = index;
    }
}

void KdTree::addInner(const Size parent, const Float splitPosition, const Size splitIdx) {
    static_assert(int(KdNode::Type::X) == 0 && int(KdNode::Type::Y) == 1 && int(KdNode::Type::Z) == 2,
        "Invalid values of KdNode::Type enum");

    nodes.emplaceBack(KdNode::Type(splitIdx));
    const Size index = nodes.size() - 1;
    InnerNode& node = (InnerNode&)nodes[index];
    ASSERT(!node.isLeaf());

#ifdef SPH_DEBUG
    node.left = node.right = -1;
#endif

    node.splitPosition = splitPosition;

    node.box = Box(); // will be computed later

    if (parent == ROOT_PARENT_NODE) {
        // no need to set up parents
        return;
    }
    InnerNode& parentNode = (InnerNode&)nodes[parent];
    if (index == parent + 1) {
        // left child
        ASSERT(parentNode.left == Size(-1));
        parentNode.left = index;
    } else {
        // right child
        ASSERT(parentNode.right == Size(-1));
        parentNode.right = index;
    }
}

void KdTree::init() {
    entireBox = Box();
    idxs.clear();
    nodes.clear();
}

bool KdTree::isSingular(const Size from, const Size to, const Size splitIdx) const {
    for (Size i = from; i < to; ++i) {
        if (values[idxs[i]][splitIdx] != values[idxs[to - 1]][splitIdx]) {
            return false;
        }
    }
    return true;
}

bool KdTree::checkBoxes(const Size from,
    const Size to,
    const Size mid,
    const Box& box1,
    const Box& box2) const {
    for (Size i = from; i < to; ++i) {
        if (i < mid && !box1.contains(values[idxs[i]])) {
            return false;
        }
        if (i >= mid && !box2.contains(values[idxs[i]])) {
            return false;
        }
    }
    return true;
}

Size KdTree::findNeighboursImpl(const Vector& r0,
    const Float radius,
    const Size refRank,
    Array<NeighbourRecord>& neighbours) const {
    const Float radiusSqr = sqr(radius);
    const Vector maxDistSqr = sqr(max(Vector(0._f), entireBox.lower() - r0, r0 - entireBox.upper()));

    // L1 norm
    const Float l1 = l1Norm(maxDistSqr);
    ProcessedNode node{ 0, maxDistSqr, l1 };

    ASSERT(nodeStack.empty()); // not sure if there can be some nodes from previous search ...

    while (node.distanceSqr < radiusSqr) {
        if (nodes[node.idx].isLeaf()) {
            // for leaf just add all
            const LeafNode& leaf = (const LeafNode&)nodes[node.idx];
            if (leaf.size() > 0) {
                const Float leafDistSqr =
                    getSqrLength(max(Vector(0._f), leaf.box.lower() - r0, r0 - leaf.box.upper()));
                if (leafDistSqr < radiusSqr) {
                    // leaf intersects the sphere
                    for (Size i = leaf.from; i < leaf.to; ++i) {
                        const Size actIndex = idxs[i];
                        const Float distSqr = getSqrLength(values[actIndex] - r0);
                        if (rankH[actIndex] < refRank && distSqr < radiusSqr) {
                            /// \todo order part
                            neighbours.push(NeighbourRecord{ actIndex, distSqr });
                        }
                    }
                }
            }
            if (nodeStack.empty()) {
                break;
            }
            node = nodeStack.pop();
        } else {
            // inner node
            const InnerNode& inner = (InnerNode&)nodes[node.idx];
            const Size splitDimension = Size(inner.type);
            ASSERT(splitDimension < 3);
            const Float splitPosition = inner.splitPosition;
            if (r0[splitDimension] < splitPosition) {
                // process left subtree, put right on stack
                ProcessedNode right = node;
                node.idx = inner.left;

                const Float dx = splitPosition - r0[splitDimension];
                right.distanceSqr += sqr(dx) - right.sizeSqr[splitDimension];
                right.sizeSqr[splitDimension] = sqr(dx);
                if (right.distanceSqr < radiusSqr) {
                    const InnerNode& next = (const InnerNode&)nodes[right.idx];
                    right.idx = next.right;
                    nodeStack.push(right);
                }
            } else {
                // process right subtree, put left on stack
                ProcessedNode left = node;
                node.idx = inner.right;
                const Float dx = splitPosition - r0[splitDimension];
                left.distanceSqr += sqr(dx) - left.sizeSqr[splitDimension];
                left.sizeSqr[splitDimension] = sqr(dx);
                if (left.distanceSqr < radiusSqr) {
                    const InnerNode& next = (const InnerNode&)nodes[left.idx];
                    left.idx = next.left;
                    nodeStack.push(left);
                }
            }
        }
    }

    return neighbours.size();
}

Size KdTree::findNeighbours(const Size index,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> flags,
    const Float UNUSED(error)) const {
    neighbours.clear();
    const Size refRank = (flags.has(FinderFlags::FIND_ONLY_SMALLER_H)) ? rankH[index] : values.size();

    const Vector& r0 = values[index];
    return this->findNeighboursImpl(r0, radius, refRank, neighbours);
}

Size KdTree::findNeighbours(const Vector& position,
    const Float radius,
    Array<NeighbourRecord>& neighbours,
    Flags<FinderFlags> UNUSED(flags),
    const Float UNUSED(error)) const {
    neighbours.clear();
    if (SPH_UNLIKELY(values.empty())) {
        return 0;
    }
    return this->findNeighboursImpl(position, radius, values.size(), neighbours);
}

bool KdTree::sanityCheck() const {
    if (values.size() != idxs.size()) {
        return false;
    }

    // check bounding box
    for (const Vector& v : values) {
        if (!entireBox.contains(v)) {
            return false;
        }
    }

    // check node connectivity
    Size counter = 0;
    std::set<Size> indices;

    Function<bool(const Size idx)> countNodes([this, &indices, &counter, &countNodes](const Size idx) {
        // count this
        counter++;

        // check index validity
        if (idx >= nodes.size()) {
            return false;
        }

        // if inner node, count children
        if (!nodes[idx].isLeaf()) {
            const InnerNode& inner = (const InnerNode&)nodes[idx];
            // left node should always be parent + 1 (pointless to store, just for checking)
            if (inner.left != idx + 1) {
                return false;
            }
            return countNodes(inner.left) && countNodes(inner.right);
        } else {
            // check that all points fit inside the bounding box of the leaf
            const LeafNode& leaf = (const LeafNode&)nodes[idx];
            if (leaf.to - leaf.from <= 1) {
                // empty leaf?
                return false;
            }
            for (Size i = leaf.from; i < leaf.to; ++i) {
                if (!leaf.box.contains(values[idxs[i]])) {
                    return false;
                }
                if (indices.find(i) != indices.end()) {
                    // child referenced twice?
                    return false;
                }
                indices.insert(i);
            }
        }
        return true;
    });
    if (!countNodes(0)) {
        // invalid index encountered
        return false;
    }
    // we should count exactly nodes.size()
    if (counter != nodes.size()) {
        return false;
    }
    // each index should have been inserted exactly once
    Size i = 0;
    for (Size idx : indices) {
        // std::set is sorted, so we can check sequentially
        if (idx != i) {
            return false;
        }
        ++i;
    }
    return true;
}

NAMESPACE_SPH_END
