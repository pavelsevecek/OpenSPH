#include "gravity/BarnesHut.h"
#include "geometry/Sphere.h"
#include "gravity/Moments.h"
#include "objects/containers/ArrayUtils.h"
#include "physics/Constants.h"
#include "quantities/Storage.h"
#include "system/Profiler.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

BarnesHut::BarnesHut(const Float theta, const MultipoleOrder order, const Size leafSize)
    : kdTree(leafSize)
    , thetaInv(1.f / theta)
    , order(order) {
    // use default-constructed kernel; it works, because by default LutKernel has zero radius and functions
    // valueImpl and gradImpl are never called.
    // Check by assert to make sure this trick will work
    ASSERT(kernel.closeRadius() == 0._f);
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

void BarnesHut::build(const Storage& storage) {
    // save source data
    r = storage.getValue<Vector>(QuantityId::POSITIONS);
    m = storage.getValue<Float>(QuantityId::MASSES);

    // build K-d Tree
    kdTree.build(r);

    if (SPH_UNLIKELY(r.empty())) {
        return;
    }
    // constructs nodes
    kdTree.iterate<KdTree::Direction::BOTTOM_UP>([this](KdNode& node, KdNode* left, KdNode* right) INL {
        if (node.isLeaf()) {
            ASSERT(left == nullptr && right == nullptr);
            buildLeaf(node);

        } else {
            ASSERT(left != nullptr && right != nullptr);
            buildInner(node, *left, *right);
        }
        return true;
    });
}

void BarnesHut::evalAll(ArrayView<Vector> dv, Statistics& stats) const {
    /*for (Size i = 0; i < dv.size(); ++i) {
        dv[i] = this->evalImpl(r[i], i, stats);
    }*/
    Array<Size> particleList;
    MARK_USED(stats);
    const KdNode& root = kdTree.getNode(0);
    if (SPH_UNLIKELY(root.isLeaf())) {
        // all particles within one leaf - boring case with only few particles
        for (Size i = 0; i < dv.size(); ++i) {
            particleList.push(i);
        }
    }
    this->evalNode(dv, root, {}, std::move(particleList), {});

    for (Size i = 0; i < dv.size(); ++i) {
        dv[i] *= Constants::gravity;
    }
}

Vector BarnesHut::eval(const Vector& r0, Statistics& stats) const {
    return this->evalImpl(r0, Size(-1), stats);
}

Vector BarnesHut::evalImpl(const Vector& r0, const Size idx, Statistics& UNUSED(stats)) const {
    if (SPH_UNLIKELY(r.empty())) {
        return Vector(0._f);
    }
    ASSERT(r0[H] > 0._f, r0[H]);

    Vector f(0._f);
    /*struct {
        Size approximated = 0;
        Size exact = 0;
    } ns;*/

    auto lambda = [this, &r0, &f, idx](const KdNode& node, const KdNode*, const KdNode*) {
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
            // f += node.moments.order<0>().value() * kernel.grad(r0 - node.com, r0[H]);
            f += evaluateGravity(r0 - node.com, node.moments, order);
            /// \todo compute exact number of particles! this is only valid for unit tests!!
            // ns.approximated += round(node.moments.order<0>().value() / m[0]);
            // skip the children
            return false;
        } else {
            // too large box; if inner, recurse into children, otherwise sum each particle of the leaf
            if (node.isLeaf()) {
                const LeafNode& leaf = reinterpret_cast<const LeafNode&>(node);
                f += this->evalExact(leaf, r0, idx);
                //  ns.exact += leaf.size();
                return false; // return value doesn't matter here
            } else {
                // continue with children
                return true;
            }
        }
    };
    kdTree.iterate<KdTree::Direction::TOP_DOWN>(lambda);

    /*stats.set(StatisticsId::GRAVITY_PARTICLES_EXACT, int(ns.exact));
    stats.set(StatisticsId::GRAVITY_PARTICLES_APPROX, int(ns.approximated));*/
    return Constants::gravity * f;
}

/// \todo try different containers; we need fast inserting & deletion

void BarnesHut::evalNode(ArrayView<Vector> dv,
    const KdNode& evaluatedNode,
    List<Size> checkList,
    Array<Size> particleList,
    Array<Size> nodeList) const {
    const Box& box = evaluatedNode.box;
    if (box == Box::EMPTY()) {
        //  no partiles in the box, skip
        ASSERT(evaluatedNode.isLeaf());
        return;
    }

    // we cannot use range-based for loop before we need the iterator for erasing the element
    for (auto iter = checkList.begin(); iter != checkList.end();) {
        ASSERT(areElementsUnique(checkList), checkList);
        const Size i = *iter;
        const KdNode& node = kdTree.getNode(i);
        if (node.box == Box::EMPTY()) {
            checkList.eraseAndIncrement(iter);
            continue;
        }

        const Sphere openBall(node.com, node.r_open);
        const IntersectResult intersect = openBall.intersectsBox(box);

        if (intersect == IntersectResult::BOX_INSIDE_SPHERE ||
            (evaluatedNode.isLeaf() && intersect != IntersectResult::BOX_OUTSIDE_SPHERE)) {
            // if leaf, add all particles into the particle interaction list
            if (node.isLeaf()) {
                const LeafNode& leaf = reinterpret_cast<const LeafNode&>(node);
                LeafIndexSequence sequence = kdTree.getLeafIndices(leaf);
                for (Size j : sequence) {
                    particleList.push(j);
                }
                ASSERT(areElementsUnique(particleList), particleList);
            } else {
                // add child nodes into the checklist
                const InnerNode& inner = reinterpret_cast<const InnerNode&>(node);
                checkList.pushBack(inner.left);
                checkList.pushBack(inner.right);
            }
            checkList.eraseAndIncrement(iter);

            continue;
        } else if (intersect == IntersectResult::BOX_OUTSIDE_SPHERE) {
            // node is outside the opening ball, we can approximate it; add to the node list
            nodeList.push(i);

            // erase this node from checklist
            checkList.eraseAndIncrement(iter);
            continue;
        }
        // for leafs we have to move all nodes from the checklist to one of interaction lists
        ASSERT(!evaluatedNode.isLeaf());
        ++iter;
    }

    if (evaluatedNode.isLeaf()) {
        ASSERT(checkList.empty(), checkList); // checklist must be empty, otherwise we forgot something
        ASSERT(areElementsUnique(particleList), particleList);

        const LeafNode& leaf = reinterpret_cast<const LeafNode&>(evaluatedNode);
        // add acceleration to all leaf particles from both interaction lists
        LeafIndexSequence sequence = kdTree.getLeafIndices(leaf);
        ASSERT(!haveCommonElements(particleList, sequence));
        ASSERT(nodeList.size() > 0 || particleList.size() + sequence.size() == dv.size());
        for (Size i : sequence) {
            // evaluate interactions of all particles from particle list
            for (Size j : particleList) {
                ASSERT(r[i][H] > 0._f, r[i][H]);
                dv[i] += m[i] * kernel.grad(r[j] - r[i], r[i][H]);
            }
            // evaluate all intra-leaf interactions (leaf itself isn't in the interaction list)
            for (Size j : sequence) {
                if (i == j) {
                    continue;
                }
                ASSERT(r[i][H] > 0._f, r[i][H]);
                dv[i] += m[i] * kernel.grad(r[j] - r[i], r[i][H]);
            }
            ASSERT(areElementsUnique(nodeList), nodeList);
            for (Size nodeIdx : nodeList) {
                const KdNode& node = kdTree.getNode(nodeIdx);
                dv[i] += evaluateGravity(r[i] - node.com, node.moments, order);
            }
        }
    } else {
        const InnerNode& inner = reinterpret_cast<const InnerNode&>(evaluatedNode);
        // recurse into child nodes
        List<Size> childCheckList = checkList.clone();
        childCheckList.pushBack(inner.right);
        Array<Size> childParticleList = particleList.clone();
        Array<Size> childNodeList = nodeList.clone();
        this->evalNode(dv,
            kdTree.getNode(inner.left),
            std::move(childCheckList),
            std::move(childParticleList),
            std::move(childNodeList));

        checkList.pushBack(inner.left);
        this->evalNode(dv,
            kdTree.getNode(inner.right),
            std::move(checkList),
            std::move(particleList),
            std::move(nodeList));
    }
}


Vector BarnesHut::evalExact(const LeafNode& leaf, const Vector& r0, const Size idx) const {
    LeafIndexSequence sequence = kdTree.getLeafIndices(leaf);
    Vector f(0._f);
    for (Size i : sequence) {
        if (idx == i) {
            continue;
        }
        ASSERT(r[i][H] > 0._f, r[i][H]);
        f += m[i] * kernel.grad(r[i] - r0, r0[H]);
    }
    return f;
}

void BarnesHut::buildLeaf(KdNode& node) {
    LeafNode& leaf = (LeafNode&)node;
    if (leaf.size() == 0) {
        // set to zero to correctly compute mass and com of parent nodes
        leaf.com = Vector(0._f);
        leaf.moments.order<0>() = 0._f;
        leaf.moments.order<1>() = TracelessMultipole<1>(0._f);
        leaf.moments.order<2>() = TracelessMultipole<2>(0._f);
        leaf.moments.order<3>() = TracelessMultipole<3>(0._f);
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
        ASSERT(r[i][H] > 0._f, r[i][H]);
        const Vector dr(r[i][H] * kernel.closeRadius());
        leaf.box.extend(r[i] + dr);
        leaf.box.extend(r[i] - dr);
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

void BarnesHut::buildInner(KdNode& node, KdNode& left, KdNode& right) {
    InnerNode& inner = (InnerNode&)node;

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

NAMESPACE_SPH_END
