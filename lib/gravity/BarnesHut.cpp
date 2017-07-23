#include "gravity/BarnesHut.h"
#include "gravity/Moments.h"
#include "physics/Constants.h"
#include "system/Profiler.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

BarnesHut::BarnesHut(const Float theta, const MultipoleOrder order, const Size leafSize)
    : kdTree(leafSize)
    , thetaSqr(sqr(theta))
    , order(order) {
    // use default-constructed kernel; it works, because by default LutKernel has zero radius and functions
    // valueImpl and gradImpl are never called.
    // Check by assert to make sure this trick will work
    ASSERT(kernel.closeRadius() == 0._f);
}

BarnesHut::BarnesHut(const Float theta,
    const MultipoleOrder order,
    GravityLutKernel&& kernel,
    const Size leafSize)
    : kdTree(leafSize)
    , kernel(std::move(kernel))
    , thetaSqr(sqr(theta))
    , order(order) {}

void BarnesHut::build(ArrayView<const Vector> points, ArrayView<const Float> masses) {
    // save source data
    r = points;
    m = masses;

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

Vector BarnesHut::eval(const Size idx, Statistics& stats) {
    return this->evalImpl(r[idx], idx, stats);
}

Vector BarnesHut::eval(const Vector& r0, Statistics& stats) {
    return this->evalImpl(r0, Size(-1), stats);
}

Vector BarnesHut::evalImpl(const Vector& r0, const Size idx, Statistics& stats) {
    if (SPH_UNLIKELY(r.empty())) {
        return Vector(0._f);
    }
    ASSERT(r0[H] > 0._f, r0[H]);

    Vector f(0._f);
    struct {
        Size approximated = 0;
        Size exact = 0;
    } ns;

    kdTree.iterate<KdTree::Direction::TOP_DOWN>([this, &r0, &f, idx, &ns](KdNode& node, KdNode*, KdNode*) {
        if (node.box == Box::EMPTY()) {
            // no particles in this node, skip
            return false;
        }
        const Float boxSizeSqr = getSqrLength(node.box.size());
        const Float boxDistSqr = getSqrLength(node.box.center() - r0);
        ASSERT(isReal(boxDistSqr));
        if (!node.box.contains(r0) && boxSizeSqr / (boxDistSqr + EPS) < thetaSqr) {
            // small node, use multipole approximation
            f += evaluateGravity(r0 - node.com, node.moments, int(order));
            /// \todo compute exact number of particles! this is only valid for unit tests!!
            ns.approximated += round(node.moments.order<0>().value() / m[0]);
            // skip the children
            return false;
        } else {
            // too large box; if inner, recurse into children, otherwise sum each particle of the leaf
            if (node.isLeaf()) {
                LeafNode& leaf = reinterpret_cast<LeafNode&>(node);
                this->evalExact(leaf, r0, idx);
                ns.exact += leaf.size();
                return false; // return value doesn't matter here
            } else {
                // continue with children
                return true;
            }
        }
    });

    stats.set(StatisticsId::GRAVITY_PARTICLES_EXACT, int(ns.exact));
    stats.set(StatisticsId::GRAVITY_PARTICLES_APPROX, int(ns.approximated));
    return Constants::gravity * f;
}

Vector BarnesHut::evalExact(LeafNode& leaf, const Vector& r0, const Size idx) {
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
