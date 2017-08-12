#pragma once

#include "gravity/AbstractGravity.h"
#include "objects/containers/List.h"
#include "objects/finders/KdTree.h"
#include "sph/kernel/GravityKernel.h"

NAMESPACE_SPH_BEGIN

enum class MultipoleOrder;

/// \brief Multipole approximation of distance particle.
class BarnesHut : public Abstract::Gravity {
protected:
    /// Source data
    ArrayView<const Vector> r;
    ArrayView<const Float> m;

    /// K-d tree storing gravitational moments
    KdTree kdTree;

    /// Kernel used to evaluate gravity of close particles
    GravityLutKernel kernel;

    /// Inverted value of the opening angle for multipole approximation (in radians)
    Float thetaInv;

    /// Order of multipole approximation
    MultipoleOrder order;

public:
    /// Constructs the Barnes-Hut gravity assuming point-like particles (with zero radius).
    /// \param theta Opening angle; lower value means higher precision, but slower computation
    /// \param order Order of multipole approximation
    /// \param leafSize Maximum number of particles in a leaf
    BarnesHut(const Float theta, const MultipoleOrder order, const Size leafSize = 20);

    /// Constructs the Barnes-Hut gravity with given smoothing kernel
    /// \param theta Opening angle; lower value means higher precision, but slower computation
    /// \param order Order of multipole approximation
    /// \param leafSize Maximum number of particles in a leaf
    BarnesHut(const Float theta,
        const MultipoleOrder order,
        GravityLutKernel&& kernel,
        const Size leafSize = 20);

    /// Masses of particles must be strictly positive, otherwise center of mass would be undefined.
    virtual void build(const Storage& storage) override;

    virtual void evalAll(ArrayView<Vector> dv, Statistics& stats) const override;

    virtual void evalAll(ThreadPool& pool,
        const ThreadLocal<ArrayView<Vector>>& dv,
        Statistics& stats) const override;

    virtual Vector eval(const Vector& r0, Statistics& stats) const override;

    /// Returns the multipole moments computed from root node.
    MultipoleExpansion<3> getMoments() const;

protected:
    /// Evaluates the gravity at a single point in space.
    Vector evalImpl(const Vector& r0, const Size idx, Statistics& stats) const;

    /// Helper task for parallelization of treewalk
    class NodeTask;

    /// Data passed into each node during treewalk
    struct TreeWalkState {
        /// Indices of nodes that need to be checked for intersections with opening ball of the evaluated
        /// node. If the opening ball does not intersect the node box, the node is moved into the node
        /// interaction list. If the node box is contained within the opening ball, the node is moved into the
        /// particle interaction list.
        List<Size> checkList;

        /// Indices of nodes where the exact (pair-wise) gravity solution have to be used.
        Array<Size> particleList;

        /// Indices of nodes that shall be evaluated using multipole approximation.
        Array<Size> nodeList;

        /// Clones all lists in the state object
        TreeWalkState clone() const;
    };

    /// \brief Performs a recursive treewalk evaluating gravity for all particles.
    ///
    /// Treewalk starts at given node and subsequently calls \ref evalNode for children nodes. Function moves
    /// nodes from checkList into interaction lists and in case the node is a leaf, gravity is computed using
    /// constructed interaction lists.
    /// \param pool Thread pool used for parallelization
    /// \param dv Output thread-local buffer where computed accelerations are stored
    /// \param node Index of the first node evaluated by the function
    void evalNode(ThreadPool& pool,
        const ThreadLocal<ArrayView<Vector>>& dv,
        const Size nodeIdx,
        TreeWalkState data) const;

    Vector evalExact(const LeafNode& node, const Vector& r0, const Size idx) const;

    void buildLeaf(KdNode& node);

    void buildInner(KdNode& node, KdNode& left, KdNode& right);
};


NAMESPACE_SPH_END
