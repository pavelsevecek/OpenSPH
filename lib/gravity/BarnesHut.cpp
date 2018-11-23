#include "gravity/BarnesHut.h"
#include "gravity/Moments.h"
#include "objects/geometry/Sphere.h"
#include "objects/utility/ArrayUtils.h"
#include "physics/Constants.h"
#include "quantities/Storage.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/Pool.h"

NAMESPACE_SPH_BEGIN

BarnesHut::BarnesHut(const Float theta, const MultipoleOrder order, const Size leafSize)
    : kdTree(leafSize)
    , thetaInv(1.f / theta)
    , order(order) {
    // use default-constructed kernel; it works, because by default LutKernel has zero radius and functions
    // valueImpl and gradImpl are never called.
    // Check by assert to make sure this trick will work
    ASSERT(kernel.radius() == 0._f);
    ASSERT(theta > 0._f, theta);
}

BarnesHut::BarnesHut(const Float theta,
    const MultipoleOrder order,
    GravityLutKernel&& kernel,
    const Size leafSize)
    : kdTree(leafSize)
    , kernel(std::move(kernel))
    , thetaInv(1.f / theta)
    , order(order) {
    ASSERT(theta > 0._f, theta);
}

void BarnesHut::build(IScheduler& scheduler, const Storage& storage) {
    // save source data
    r = storage.getValue<Vector>(QuantityId::POSITION);

    m.resize(r.size());
    ArrayView<const Float> masses = storage.getValue<Float>(QuantityId::MASS);
    for (Size i = 0; i < m.size(); ++i) {
        m[i] = Constants::gravity * masses[i];
    }

    // build K-d Tree; no need for rank as we are never searching neighbours
    kdTree.build(scheduler, r, FinderFlag::SKIP_RANK);

    if (SPH_UNLIKELY(r.empty())) {
        return;
    }
    // constructs nodes
    auto functor = [this](BarnesHutNode& node, BarnesHutNode* left, BarnesHutNode* right) INL {
        if (node.isLeaf()) {
            ASSERT(left == nullptr && right == nullptr);
            buildLeaf(node);

        } else {
            ASSERT(left != nullptr && right != nullptr);
            buildInner(node, *left, *right);
        }
        return true;
    };
    /// \todo sequential needed because TBB cannot wait on child tasks yet
    iterateTree<IterateDirection::BOTTOM_UP>(kdTree, SEQUENTIAL, functor);
}

void BarnesHut::evalAll(IScheduler& scheduler, ArrayView<Vector> dv, Statistics& stats) const {
    TreeWalkState data;
    TreeWalkResult result;
    SharedPtr<ITask> rootTask = scheduler.submit([this, &scheduler, dv, &data, &result] { //
        this->evalNode(scheduler, dv, 0, std::move(data), result);
    });
    rootTask->wait();

    stats.set<int>(StatisticsId::GRAVITY_NODES_APPROX, result.approximatedNodes);
    stats.set<int>(StatisticsId::GRAVITY_NODES_EXACT, result.exactNodes);
    stats.set<int>(StatisticsId::GRAVITY_NODE_COUNT, kdTree.getNodeCnt());
}


Vector BarnesHut::eval(const Vector& r0, Statistics& stats) const {
    return this->evalImpl(r0, Size(-1), stats);
}

Float BarnesHut::evalEnergy(IScheduler& UNUSED(scheduler), Statistics& UNUSED(stats)) const {
    NOT_IMPLEMENTED;
}

Vector BarnesHut::evalImpl(const Vector& r0, const Size idx, Statistics& UNUSED(stats)) const {
    if (SPH_UNLIKELY(r.empty())) {
        return Vector(0._f);
    }
    Vector f(0._f);

    auto lambda = [this, &r0, &f, idx](
                      const BarnesHutNode& node, const BarnesHutNode*, const BarnesHutNode*) {
        if (node.box == Box::EMPTY()) {
            // no particles in this node, skip
            return false;
        }
        const Float boxSizeSqr = getSqrLength(node.box.size());
        const Float boxDistSqr = getSqrLength(node.box.center() - r0);
        ASSERT(isReal(boxDistSqr));
        /// \todo
        if (!node.box.contains(r0) && boxSizeSqr / (boxDistSqr + EPS) < 1.f / sqr(thetaInv)) {
            // small node, use multipole approximation
            f += evaluateGravity(r0 - node.com, node.moments, order);

            // skip the children
            return false;
        } else {
            // too large box; if inner, recurse into children, otherwise sum each particle of the leaf
            if (node.isLeaf()) {
                const LeafNode<BarnesHutNode>& leaf = reinterpret_cast<const LeafNode<BarnesHutNode>&>(node);
                f += this->evalExact(leaf, r0, idx);
                return false; // return value doesn't matter here
            } else {
                // continue with children
                return true;
            }
        }
    };
    iterateTree<IterateDirection::TOP_DOWN>(kdTree, SEQUENTIAL, lambda);

    return f;
}

class BarnesHut::NodeTask : public Noncopyable {
private:
    const BarnesHut& bh;
    IScheduler& scheduler;
    ArrayView<Vector> dv;
    Size nodeIdx;
    BarnesHut::TreeWalkState data;
    BarnesHut::TreeWalkResult& result;

public:
    NodeTask(const BarnesHut& bh,
        IScheduler& scheduler,
        ArrayView<Vector> dv,
        const Size nodeIdx,
        BarnesHut::TreeWalkState&& data,
        BarnesHut::TreeWalkResult& result)
        : bh(bh)
        , scheduler(scheduler)
        , dv(dv)
        , nodeIdx(nodeIdx)
        , data(std::move(data))
        , result(result) {}

    void operator()() {
        bh.evalNode(scheduler, dv, nodeIdx, std::move(data), result);
    }
};

BarnesHut::TreeWalkState BarnesHut::TreeWalkState::clone() const {
    TreeWalkState cloned;
    cloned.checkList = checkList.clone();
    cloned.particleList = particleList.clone();
    cloned.nodeList = nodeList.clone();
    cloned.depth = depth;
    return cloned;
}

/// \todo try different containers; we need fast inserting & deletion
void BarnesHut::evalNode(IScheduler& scheduler,
    ArrayView<Vector> dv,
    const Size evaluatedNodeIdx,
    TreeWalkState data,
    TreeWalkResult& result) const {

    const BarnesHutNode& evaluatedNode = kdTree.getNode(evaluatedNodeIdx);
    const Box& box = evaluatedNode.box;

    if (box == Box::EMPTY()) {
        // no particles in the box, skip
        /// \todo ASSERT FIRED !
        ASSERT(evaluatedNode.isLeaf());
        return;
    }

    // we cannot use range-based for loop because we need the iterator for erasing the element
    for (auto iter = data.checkList.begin(); iter != data.checkList.end();) {
        ASSERT(areElementsUnique(data.checkList), data.checkList);

        const Size idx = *iter;
        const BarnesHutNode& node = kdTree.getNode(idx);
        if (node.r_open == 0.f) {
            // either empty node or a single particle in a leaf, just add it to particle list
            ASSERT(node.isLeaf());
            data.particleList.push(idx);
            iter = data.checkList.erase(iter);
            continue;
        }

        const Sphere openBall(node.com, node.r_open);
        const IntersectResult intersect = openBall.intersectsBox(box);

        if (intersect == IntersectResult::BOX_INSIDE_SPHERE ||
            (evaluatedNode.isLeaf() && intersect != IntersectResult::BOX_OUTSIDE_SPHERE)) {
            // if leaf, add it into the particle interaction list
            if (node.isLeaf()) {
                // let's add only nodes with higher indices, the evalution is symmetric anyway
                data.particleList.push(idx);
            } else {
                // add child nodes into the checklist
                const InnerNode<BarnesHutNode>& inner =
                    reinterpret_cast<const InnerNode<BarnesHutNode>&>(node);
                data.checkList.pushBack(inner.left);
                data.checkList.pushBack(inner.right);
            }
            iter = data.checkList.erase(iter);

            continue;
        } else if (intersect == IntersectResult::BOX_OUTSIDE_SPHERE) {
            // node is outside the opening ball, we can approximate it; add to the node list
            data.nodeList.push(idx);

            // erase this node from checklist
            iter = data.checkList.erase(iter);
            continue;
        }
        // for leafs we have to move all nodes from the checklist to one of interaction lists
        ASSERT(!evaluatedNode.isLeaf());
        ++iter;
    }

    if (evaluatedNode.isLeaf()) {
        // checklist must be empty, otherwise we forgot something
        ASSERT(data.checkList.empty(), data.checkList);
        const LeafNode<BarnesHutNode>& leaf = reinterpret_cast<const LeafNode<BarnesHutNode>&>(evaluatedNode);

        // 1) evaluate the particle list:
        this->evalParticleList(leaf, data.particleList, dv);
        result.exactNodes += data.particleList.size();

        // 2) evaluate the node list
        this->evalNodeList(leaf, data.nodeList, dv);
        result.approximatedNodes += data.nodeList.size();

    } else {
        const InnerNode<BarnesHutNode>& inner =
            reinterpret_cast<const InnerNode<BarnesHutNode>&>(evaluatedNode);
        // recurse into child nodes
        data.depth++;
        // we evaluate the left one from a (possibly) different thread, we thus have to clone buffers now so
        // that we don't override the lists when evaluating different node (each node has its own lists).
        TreeWalkState childData = data.clone();
        childData.checkList.pushBack(inner.right);
        auto task = makeShared<NodeTask>(*this, scheduler, dv, inner.left, std::move(childData), result);
        if (childData.depth < maxDepth) {
            // Ad-hoc decision (see also KdTree.cpp where we do the same trick);
            // only split the build in the topmost nodes, process the bottom nodes in the same thread to avoid
            // high scheduling overhead of ThreadPool (TBBs deal with this quite well)
            //
            // The expression above can be modified to get optimal performance.
            scheduler.submit(std::move(task));
        } else {
            task();
        }

        // since we go only once through the tree (we never go 'up'), we can simply move the lists into the
        // right child and modify them for the child node
        data.checkList.pushBack(inner.left);
        this->evalNode(scheduler, dv, inner.right, std::move(data), result);
    }
}

void BarnesHut::evalParticleList(const LeafNode<BarnesHutNode>& leaf,
    ArrayView<Size> particleList,
    ArrayView<Vector> dv) const {
    // needs to symmetrize smoothing length to keep the total momentum conserved
    SymmetrizeSmoothingLengths<const GravityLutKernel&> actKernel(kernel);
    // go through all nodes in the list and compute the pair-wise interactions
    LeafIndexSequence seq1 = kdTree.getLeafIndices(leaf);
    ASSERT(areElementsUnique(particleList), particleList);
    for (Size idx : particleList) {
        // the particle lists do not have to be necessarily symmetric, we have to do each node separately
        ASSERT(idx < kdTree.getNodeCnt(), idx, kdTree.getNodeCnt());
        const BarnesHutNode& node = kdTree.getNode(idx);
        ASSERT(node.isLeaf());
        LeafIndexSequence seq2 =
            kdTree.getLeafIndices(reinterpret_cast<const LeafNode<BarnesHutNode>&>(node));
        for (Size i : seq1) {
            ASSERT(r[i][H] > 0._f, r[i][H]);
            for (Size j : seq2) {
                ASSERT(r[j][H] > 0._f, r[j][H]);
                const Vector grad = actKernel.grad(r[j], r[i]);
                dv[i] += m[j] * grad;
            }
        }
    }
    // evaluate intra-leaf interactions (the leaf itself is not included in the list)
    for (Size i : seq1) {
        for (Size j : seq1) {
            if (i == j) {
                // skip, we are doing a symmetric evaluation
                continue;
            }
            const Vector grad = actKernel.grad(r[j], r[i]);
            dv[i] += m[j] * grad;
        }
    }
}

void BarnesHut::evalNodeList(const LeafNode<BarnesHutNode>& leaf,
    ArrayView<Size> nodeList,
    ArrayView<Vector> dv) const {
    ASSERT(areElementsUnique(nodeList), nodeList);
    LeafIndexSequence seq1 = kdTree.getLeafIndices(leaf);
    for (Size idx : nodeList) {
        const BarnesHutNode& node = kdTree.getNode(idx);
        ASSERT(seq1.size() > 0);
        for (Size i : seq1) {
            dv[i] += evaluateGravity(r[i] - node.com, node.moments, order);
        }
    }
}

Vector BarnesHut::evalExact(const LeafNode<BarnesHutNode>& leaf, const Vector& r0, const Size idx) const {
    LeafIndexSequence sequence = kdTree.getLeafIndices(leaf);
    Vector f(0._f);
    for (Size i : sequence) {
        if (idx == i) {
            continue;
        }
        f += m[i] * kernel.grad(r[i] - r0, r[i][H]);
    }
    return f;
}

void BarnesHut::buildLeaf(BarnesHutNode& node) {
    LeafNode<BarnesHutNode>& leaf = (LeafNode<BarnesHutNode>&)node;
    if (leaf.size() == 0) {
        // set to zero to correctly compute mass and com of parent nodes
        leaf.com = Vector(0._f);
        leaf.moments.order<0>() = 0._f;
        leaf.moments.order<1>() = TracelessMultipole<1>(0._f);
        leaf.moments.order<2>() = TracelessMultipole<2>(0._f);
        leaf.moments.order<3>() = TracelessMultipole<3>(0._f);
        leaf.r_open = 0._f;
        return;
    }
    // compute the center of gravity (the box is already done)
    leaf.com = Vector(0._f);
    Float m_leaf = 0._f;
    LeafIndexSequence sequence = kdTree.getLeafIndices(leaf);
    for (Size i : sequence) {
        leaf.com += m[i] * r[i];
        m_leaf += m[i];
        // extend the bounding box, given particle radius
        /// \todo this slows down the simulation way too much, approximating by point particles
        // ASSERT(r[i][H] > 0._f, r[i][H]);
        // const Vector dr(r[i][H] * kernel.radius());
        // leaf.box.extend(r[i] + dr);
        // leaf.box.extend(r[i] - dr);
        leaf.box.extend(r[i]);
    }
    ASSERT(m_leaf > 0._f, m_leaf);
    leaf.com /= m_leaf;
    ASSERT(isReal(leaf.com) && getLength(leaf.com) < LARGE, leaf.com);

    // compute opening radius using Eq. (2.36) of Stadel Phd Thesis
    const Vector r_max = max(leaf.com - leaf.box.lower(), leaf.box.upper() - leaf.com);
    ASSERT(minElement(r_max) >= 0._f, r_max);
    leaf.r_open = 2._f / sqrt(3._f) * thetaInv * getLength(r_max);

    // compute gravitational moments from individual particles
    // M0 is a sum of particle masses, M1 is a dipole moment = zero around center of mass
    ASSERT(computeMultipole<0>(r, m, leaf.com, sequence).value() == m_leaf);
    const Multipole<2> m2 = computeMultipole<2>(r, m, leaf.com, sequence);
    const Multipole<3> m3 = computeMultipole<3>(r, m, leaf.com, sequence);

    // compute traceless tensors to reduce number of independent components
    const TracelessMultipole<2> q2 = computeReducedMultipole(m2);
    const TracelessMultipole<3> q3 = computeReducedMultipole(m3);

    // save the moments to the leaf
    leaf.moments.order<0>() = m_leaf;
    leaf.moments.order<1>() = TracelessMultipole<1>(0._f);
    leaf.moments.order<2>() = q2;
    leaf.moments.order<3>() = q3;

    // sanity check
    /// \todo fails, but appers to be only due to round-off errors
    // ASSERT(leaf.size() > 1 || q2 == TracelessMultipole<2>(0._f));
    // ASSERT(leaf.size() > 1 || q3 == TracelessMultipole<3>(0._f));
}

void BarnesHut::buildInner(BarnesHutNode& node, BarnesHutNode& left, BarnesHutNode& right) {
    InnerNode<BarnesHutNode>& inner = (InnerNode<BarnesHutNode>&)node;

    // update bounding box
    inner.box = Box::EMPTY();
    inner.box.extend(left.box);
    inner.box.extend(right.box);

    // update center of mass
    const Float ml = left.moments.order<0>();
    const Float mr = right.moments.order<0>();

    // check for empty node
    if (ml + mr == 0._f) {
        // set to zero to correctly compute sum and com of parent nodes
        inner.com = Vector(0._f);
        inner.moments.order<0>() = 0._f;
        inner.moments.order<1>() = TracelessMultipole<1>(0._f);
        inner.moments.order<2>() = TracelessMultipole<2>(0._f);
        inner.moments.order<3>() = TracelessMultipole<3>(0._f);
        inner.r_open = 0._f;
        return;
    }

    inner.com = (ml * left.com + mr * right.com) / (ml + mr);
    ASSERT(isReal(inner.com) && getLength(inner.com) < LARGE, inner.com);

    // compute opening radius
    const Vector r_max = max(inner.com - inner.box.lower(), inner.box.upper() - inner.com);
    ASSERT(minElement(r_max) >= 0._f, r_max);
    inner.r_open = 2._f / sqrt(3._f) * thetaInv * getLength(r_max);

    inner.moments.order<0>() = ml + mr;

    // we already computed moments of children nodes, sum up using parallel axis theorem
    Vector d = left.com - inner.com;
    TracelessMultipole<1>& Ml1 = left.moments.order<1>();
    TracelessMultipole<2>& Ml2 = left.moments.order<2>();
    TracelessMultipole<3>& Ml3 = left.moments.order<3>();
    inner.moments.order<1>() = parallelAxisTheorem(Ml1, ml, d);
    inner.moments.order<2>() = parallelAxisTheorem(Ml2, ml, d);
    inner.moments.order<3>() = parallelAxisTheorem(Ml3, Ml2, ml, d);

    d = right.com - inner.com;
    TracelessMultipole<1>& Mr1 = right.moments.order<1>();
    TracelessMultipole<2>& Mr2 = right.moments.order<2>();
    TracelessMultipole<3>& Mr3 = right.moments.order<3>();
    inner.moments.order<1>() += parallelAxisTheorem(Mr1, mr, d);
    inner.moments.order<2>() += parallelAxisTheorem(Mr2, mr, d);
    inner.moments.order<3>() += parallelAxisTheorem(Mr3, Mr2, mr, d);
}

MultipoleExpansion<3> BarnesHut::getMoments() const {
    return kdTree.getNode(0).moments;
}

RawPtr<const IBasicFinder> BarnesHut::getFinder() const {
    ASSERT(kdTree.getNodeCnt() > 0 && kdTree.sanityCheck(), kdTree.sanityCheck());
    return &kdTree;
}

NAMESPACE_SPH_END
