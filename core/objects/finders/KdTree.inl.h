#include "objects/finders/KdTree.h" // including the header just to make syntax highlighting work and code navigation

NAMESPACE_SPH_BEGIN

enum class KdChild {
    LEFT = 0,
    RIGHT = 1,
};

template <typename TNode, typename TMetric>
void KdTree<TNode, TMetric>::buildImpl(IScheduler& scheduler, ArrayView<const Vector> points) {
    VERBOSE_LOG

    static_assert(sizeof(LeafNode<TNode>) == sizeof(InnerNode<TNode>), "Sizes of nodes must match");

    // clean the current tree
    const Size currentCnt = nodes.size();
    this->init();

    for (const auto& i : iterateWithIndex(points)) {
        entireBox.extend(i.value());
        idxs.push(i.index());
    }

    if (SPH_UNLIKELY(points.empty())) {
        return;
    }

    const Size nodeCnt = max(2 * points.size() / config.leafSize + 1, currentCnt);
    nodes.resize(nodeCnt);

    SharedPtr<ITask> rootTask = scheduler.submit([this, &scheduler, points] {
        this->buildTree(scheduler, ROOT_PARENT_NODE, KdChild(-1), 0, points.size(), entireBox, 0, 0);
    });
    rootTask->wait();

    // shrink nodes to only the constructed ones
    nodes.resize(nodeCounter);

    SPH_ASSERT(this->sanityCheck(), this->sanityCheck().error());
}

template <typename TNode, typename TMetric>
void KdTree<TNode, TMetric>::buildTree(IScheduler& scheduler,
    const Size parent,
    const KdChild child,
    const Size from,
    const Size to,
    const Box& box,
    const Size slidingCnt,
    const Size depth) {

    Box box1, box2;
    Vector boxSize = box.size();

    // split by the dimension of largest extent
    Size splitIdx = argMax(boxSize);

    bool slidingMidpoint = false;
    bool degeneratedBox = false;

    if (to - from <= config.leafSize) {
        // enough points to fit inside one leaf
        this->addLeaf(parent, child, from, to);
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
                    SPH_ASSERT(false, "Too many overlapping points, something is probably wrong ...");
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
                for (; n1 < int(to) && this->values[idxs[n1]][splitIdx] <= splitPosition; ++n1)
                    ;
                for (; n2 >= int(from) && this->values[idxs[n2]][splitIdx] >= splitPosition; --n2)
                    ;
                if (n1 >= n2) {
                    break;
                }
            }

            if (n1 == int(from)) {
                Size idx = from;
                splitPosition = this->values[idxs[from]][splitIdx];
                for (Size i = from + 1; i < to; ++i) {
                    const Float x1 = this->values[idxs[i]][splitIdx];
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
                splitPosition = this->values[idxs[from]][splitIdx];
                for (Size i = from + 1; i < to; ++i) {
                    const Float x2 = this->values[idxs[i]][splitIdx];
                    if (x2 > splitPosition) {
                        idx = i;
                        splitPosition = x2;
                    }
                }
                std::swap(idxs[to - 1], idxs[idx]);
                n1--;
                slidingMidpoint = true;
            }

            tie(box1, box2) = box.split(splitIdx, splitPosition);
        } else {
            n1 = (from + to) >> 1;
            // do quick select to sort elements around the midpoint
            Iterator<Size> iter = idxs.begin();
            if (!degeneratedBox) {
                std::nth_element(iter + from, iter + n1, iter + to, [this, splitIdx](Size i1, Size i2) {
                    return this->values[i1][splitIdx] < this->values[i2][splitIdx];
                });
            }

            tie(box1, box2) = box.split(splitIdx, this->values[idxs[n1]][splitIdx]);
        }

        // sanity check
        SPH_ASSERT(this->checkBoxes(from, to, n1, box1, box2));

        // add inner node and connect it to the parent
        const Size index = this->addInner(parent, child, splitPosition, splitIdx);

        // recurse to left and right subtree
        const Size nextSlidingCnt = slidingMidpoint ? slidingCnt + 1 : 0;
        auto processRightSubTree = [this, &scheduler, index, to, n1, box2, nextSlidingCnt, depth] {
            this->buildTree(scheduler, index, KdChild::RIGHT, n1, to, box2, nextSlidingCnt, depth + 1);
        };
        if (depth < config.maxParallelDepth) {
            // ad hoc decision - split the build only for few topmost nodes, there is no point in splitting
            // the work for child node in the bottom, it would only overburden the ThreadPool.
            scheduler.submit(processRightSubTree);
        } else {
            // otherwise simply process both subtrees in the same thread
            processRightSubTree();
        }
        this->buildTree(scheduler, index, KdChild::LEFT, from, n1, box1, nextSlidingCnt, depth + 1);
    }
}

template <typename TNode, typename TMetric>
void KdTree<TNode, TMetric>::addLeaf(const Size parent, const KdChild child, const Size from, const Size to) {
    const Size index = nodeCounter++;
    if (index >= nodes.size()) {
        // needs more nodes than estimated; allocate up to 2x more than necessary to avoid frequent
        // reallocations
        nodesMutex.lock();
        nodes.resize(max(2 * index, nodes.size()));
        nodesMutex.unlock();
    }

    nodesMutex.lock_shared();
    auto releaseLock = finally([this] { nodesMutex.unlock_shared(); });

    LeafNode<TNode>& node = (LeafNode<TNode>&)nodes[index];
    node.type = KdNode::Type::LEAF;
    SPH_ASSERT(node.isLeaf());

#ifdef SPH_DEBUG
    node.from = node.to = -1;
#endif

    node.from = from;
    node.to = to;

    // find the bounding box of the leaf
    Box box;
    for (Size i = from; i < to; ++i) {
        box.extend(this->values[idxs[i]]);
    }
    node.box = box;

    if (parent == ROOT_PARENT_NODE) {
        return;
    }
    InnerNode<TNode>& parentNode = (InnerNode<TNode>&)nodes[parent];
    SPH_ASSERT(!parentNode.isLeaf());
    if (child == KdChild::LEFT) {
        // left child
        parentNode.left = index;
    } else {
        SPH_ASSERT(child == KdChild::RIGHT);
        // right child
        parentNode.right = index;
    }
}

template <typename TNode, typename TMetric>
Size KdTree<TNode, TMetric>::addInner(const Size parent,
    const KdChild child,
    const Float splitPosition,
    const Size splitIdx) {
    static_assert(int(KdNode::Type::X) == 0 && int(KdNode::Type::Y) == 1 && int(KdNode::Type::Z) == 2,
        "Invalid values of KdNode::Type enum");

    const Size index = nodeCounter++;
    if (index >= nodes.size()) {
        // needs more nodes than estimated; allocate up to 2x more than necessary to avoid frequent
        // reallocations
        nodesMutex.lock();
        nodes.resize(max(2 * index, nodes.size()));
        nodesMutex.unlock();
    }

    nodesMutex.lock_shared();
    auto releaseLock = finally([this] { nodesMutex.unlock_shared(); });
    InnerNode<TNode>& node = (InnerNode<TNode>&)nodes[index];
    node.type = KdNode::Type(splitIdx);
    SPH_ASSERT(!node.isLeaf());

#ifdef SPH_DEBUG
    node.left = node.right = -1;
    node.box = Box(); // will be computed later
#endif

    node.splitPosition = float(splitPosition);

    if (parent == ROOT_PARENT_NODE) {
        // no need to set up parents
        return index;
    }
    InnerNode<TNode>& parentNode = (InnerNode<TNode>&)nodes[parent];
    if (child == KdChild::LEFT) {
        // left child
        SPH_ASSERT(parentNode.left == Size(-1));
        parentNode.left = index;
    } else {
        SPH_ASSERT(child == KdChild::RIGHT);
        // right child
        SPH_ASSERT(parentNode.right == Size(-1));
        parentNode.right = index;
    }

    return index;
}

template <typename TNode, typename TMetric>
void KdTree<TNode, TMetric>::init() {
    entireBox = Box();
    idxs.clear();
    nodes.clear();
    nodeCounter = 0;
}

template <typename TNode, typename TMetric>
bool KdTree<TNode, TMetric>::isSingular(const Size from, const Size to, const Size splitIdx) const {
    for (Size i = from; i < to; ++i) {
        if (this->values[idxs[i]][splitIdx] != this->values[idxs[to - 1]][splitIdx]) {
            return false;
        }
    }
    return true;
}

template <typename TNode, typename TMetric>
bool KdTree<TNode, TMetric>::checkBoxes(const Size from,
    const Size to,
    const Size mid,
    const Box& box1,
    const Box& box2) const {
    for (Size i = from; i < to; ++i) {
        if (i < mid && !box1.contains(this->values[idxs[i]])) {
            return false;
        }
        if (i >= mid && !box2.contains(this->values[idxs[i]])) {
            return false;
        }
    }
    return true;
}

/// \brief Object used during traversal.
///
/// Holds an index of the node and squared distance of the bounding box.
struct ProcessedNode {
    /// Index into the nodeStack array. We cannot use pointers because the array might get reallocated.
    Size idx;

    Vector sizeSqr;

    Float distanceSqr;
};

/// \brief Cached stack to avoid reallocation
///
/// It is thread_local to allow using KdTree from multiple threads
extern thread_local Array<ProcessedNode> nodeStack;

template <typename TNode, typename TMetric>
template <bool FindAll>
Size KdTree<TNode, TMetric>::find(const Vector& r0,
    const Size index,
    const Float radius,
    Array<NeighborRecord>& neighbors) const {

    SPH_ASSERT(neighbors.empty());
    const Float radiusSqr = sqr(radius);
    const Vector maxDistSqr = sqr(max(Vector(0._f), entireBox.lower() - r0, r0 - entireBox.upper()));

    // L1 norm
    const Float l1 = l1Norm(maxDistSqr);
    ProcessedNode node{ 0, maxDistSqr, l1 };

    SPH_ASSERT(nodeStack.empty()); // not sure if there can be some nodes from previous search ...

    TMetric metric;
    while (node.distanceSqr < radiusSqr) {
        if (nodes[node.idx].isLeaf()) {
            // for leaf just add all
            const LeafNode<TNode>& leaf = (const LeafNode<TNode>&)nodes[node.idx];
            if (leaf.size() > 0) {
                const Float leafDistSqr =
                    metric(max(Vector(0._f), leaf.box.lower() - r0, r0 - leaf.box.upper()));
                if (leafDistSqr < radiusSqr) {
                    // leaf intersects the sphere
                    for (Size i = leaf.from; i < leaf.to; ++i) {
                        const Size actIndex = idxs[i];
                        const Float distSqr = metric(this->values[actIndex] - r0);
                        if (distSqr < radiusSqr && (FindAll || this->rank[actIndex] < this->rank[index])) {
                            /// \todo order part
                            neighbors.push(NeighborRecord{ actIndex, distSqr });
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
            const InnerNode<TNode>& inner = (InnerNode<TNode>&)nodes[node.idx];
            const Size splitDimension = Size(inner.type);
            SPH_ASSERT(splitDimension < 3);
            const Float splitPosition = inner.splitPosition;
            if (r0[splitDimension] < splitPosition) {
                // process left subtree, put right on stack
                ProcessedNode right = node;
                node.idx = inner.left;

                const Float dx = splitPosition - r0[splitDimension];
                right.distanceSqr += sqr(dx) - right.sizeSqr[splitDimension];
                right.sizeSqr[splitDimension] = sqr(dx);
                if (right.distanceSqr < radiusSqr) {
                    const InnerNode<TNode>& next = (const InnerNode<TNode>&)nodes[right.idx];
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
                    const InnerNode<TNode>& next = (const InnerNode<TNode>&)nodes[left.idx];
                    left.idx = next.left;
                    nodeStack.push(left);
                }
            }
        }
    }

    return neighbors.size();
}

template <typename TNode, typename TMetric>
Outcome KdTree<TNode, TMetric>::sanityCheck() const {
    if (this->values.size() != idxs.size()) {
        return makeFailed("Number of values does not match the number of indices");
    }

    // check bounding box
    for (const Vector& v : this->values) {
        if (!entireBox.contains(v)) {
            return makeFailed("Points are not strictly within the bounding box");
        }
    }

    // check node connectivity
    Size counter = 0;
    std::set<Size> indices;

    Function<Outcome(const Size idx)> countNodes = [this, &indices, &counter, &countNodes](
                                                       const Size idx) -> Outcome {
        // count this
        counter++;

        // check index validity
        if (idx >= nodes.size()) {
            return makeFailed("Invalid index found: ", idx, " (", nodes.size(), ")");
        }

        // if inner node, count children
        if (!nodes[idx].isLeaf()) {
            const InnerNode<TNode>& inner = (const InnerNode<TNode>&)nodes[idx];
            return countNodes(inner.left) && countNodes(inner.right);
        } else {
            // check that all points fit inside the bounding box of the leaf
            const LeafNode<TNode>& leaf = (const LeafNode<TNode>&)nodes[idx];
            if (leaf.to == leaf.from) {
                // empty leaf?
                return makeFailed("Empty leaf: ", leaf.to);
            }
            for (Size i = leaf.from; i < leaf.to; ++i) {
                if (!leaf.box.contains(this->values[idxs[i]])) {
                    return makeFailed("Leaf points do not fit inside the bounding box");
                }
                if (indices.find(i) != indices.end()) {
                    // child referenced twice?
                    return makeFailed("Index repeated: ", i);
                }
                indices.insert(i);
            }
        }
        return SUCCESS;
    };
    const Outcome result = countNodes(0);
    if (!result) {
        return result;
    }
    // we should count exactly nodes.size()
    if (counter != nodes.size()) {
        return makeFailed("Unexpected number of nodes: ", counter, " == ", nodes.size());
    }
    // each index should have been inserted exactly once
    Size i = 0;
    for (Size idx : indices) {
        // std::set is sorted, so we can check sequentially
        if (idx != i) {
            return makeFailed("Invalid index: ", idx, " == ", i);
        }
        ++i;
    }
    return SUCCESS;
}

template <IterateDirection Dir, typename TNode, typename TMetric, typename TFunctor>
void iterateTree(KdTree<TNode, TMetric>& tree,
    IScheduler& scheduler,
    const TFunctor& functor,
    const Size nodeIdx,
    const Size depthLimit) {
    TNode& node = tree.getNode(nodeIdx);
    if (Dir == IterateDirection::TOP_DOWN) {
        if (node.isLeaf()) {
            functor(node, nullptr, nullptr);
        } else {
            InnerNode<TNode>& inner = reinterpret_cast<InnerNode<TNode>&>(node);
            if (!functor(inner, &tree.getNode(inner.left), &tree.getNode(inner.right))) {
                return;
            }
        }
    }
    SharedPtr<ITask> task;
    if (!node.isLeaf()) {
        InnerNode<TNode>& inner = reinterpret_cast<InnerNode<TNode>&>(node);

        const Size newDepth = depthLimit == 0 ? 0 : depthLimit - 1;
        auto iterateRightSubtree = [&tree, &scheduler, &functor, &inner, newDepth] {
            iterateTree<Dir>(tree, scheduler, functor, inner.right, newDepth);
        };
        if (newDepth > 0) {
            task = scheduler.submit(iterateRightSubtree);
        } else {
            iterateRightSubtree();
        }
        iterateTree<Dir>(tree, scheduler, functor, inner.left, newDepth);
    }
    if (task) {
        task->wait();
    }
    if (Dir == IterateDirection::BOTTOM_UP) {
        if (node.isLeaf()) {
            functor(node, nullptr, nullptr);
        } else {
            InnerNode<TNode>& inner = reinterpret_cast<InnerNode<TNode>&>(node);
            functor(inner, &tree.getNode(inner.left), &tree.getNode(inner.right));
        }
    }
}

/// \copydoc iterateTree
template <IterateDirection Dir, typename TNode, typename TMetric, typename TFunctor>
void iterateTree(const KdTree<TNode, TMetric>& tree,
    IScheduler& scheduler,
    const TFunctor& functor,
    const Size nodeIdx,
    const Size depthLimit) {
    // use non-const overload using const_cast, but call the functor with const reference
    auto actFunctor = [&functor](TNode& node, TNode* left, TNode* right)
                          INL { return functor(asConst(node), left, right); };
    iterateTree<Dir>(const_cast<KdTree<TNode, TMetric>&>(tree), scheduler, actFunctor, nodeIdx, depthLimit);
}

NAMESPACE_SPH_END
