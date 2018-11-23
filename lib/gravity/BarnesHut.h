#pragma once

/// \file BarnesHut.h
/// \brief Barnes-Hut algorithm for computation of gravitational acceleration
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/ForwardDecl.h"
#include "gravity/IGravity.h"
#include "objects/containers/List.h"
#include "objects/finders/KdTree.h"
#include "objects/geometry/Multipole.h"
#include "sph/kernel/GravityKernel.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

enum class MultipoleOrder;

struct BarnesHutNode : public KdNode {
    /// Center of mass of contained particles
    Vector com;

    /// Gravitational moments with a respect to the center of mass, using expansion to octupole order.
    MultipoleExpansion<3> moments;

    /// Opening radius of the node; see Eq. (2.36) of Stadel PhD thesis
    /// \todo can be stored as 4th component of com.
    Float r_open;

    BarnesHutNode(const Type& type)
        : KdNode(type) {
#ifdef SPH_DEBUG
        com = Vector(NAN);
        r_open = NAN;
#endif
    }
};


/// \brief Multipole approximation of distance particle.
class BarnesHut : public IGravity {
protected:
    /// View of a position buffer in the storage
    ArrayView<const Vector> r;

    /// Particle masses multiplied by gravitational constant.
    Array<Float> m;

    /// K-d tree storing gravitational moments
    KdTree<BarnesHutNode> kdTree;

    /// Kernel used to evaluate gravity of close particles
    GravityLutKernel kernel;

    /// Inverted value of the opening angle for multipole approximation (in radians)
    Float thetaInv;

    /// Order of multipole approximation
    MultipoleOrder order;

    /// Maximum depth at which the nodes evaluation is parallelized.
    ///
    /// Child nodes are then evaluated serially on the same thread.
    Size maxDepth = Size(-1);

public:
    /// \brief Constructs the Barnes-Hut gravity assuming point-like particles (with zero radius).
    ///
    /// \param theta Opening angle; lower value means higher precision, but slower computation
    /// \param order Order of multipole approximation
    /// \param leafSize Maximum number of particles in a leaf
    BarnesHut(const Float theta, const MultipoleOrder order, const Size leafSize = 20);

    /// \brief Constructs the Barnes-Hut gravity with given smoothing kernel
    ///
    /// \param theta Opening angle; lower value means higher precision, but slower computation
    /// \param order Order of multipole approximation
    /// \param leafSize Maximum number of particles in a leaf
    BarnesHut(const Float theta,
        const MultipoleOrder order,
        GravityLutKernel&& kernel,
        const Size leafSize = 20);

    /// Masses of particles must be strictly positive, otherwise center of mass would be undefined.
    virtual void build(IScheduler& pool, const Storage& storage) override;

    virtual void evalAll(IScheduler& pool, ArrayView<Vector> dv, Statistics& stats) const override;

    virtual Vector eval(const Vector& r0, Statistics& stats) const override;

    virtual Float evalEnergy(IScheduler& scheduler, Statistics& stats) const override;

    virtual RawPtr<const IBasicFinder> getFinder() const override;

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

        /// Current depth in the tree; root node has depth equal to zero.
        Size depth = 0;

        /// Clones all lists in the state object
        TreeWalkState clone() const;
    };

    struct TreeWalkResult {
        /// Number of nodes approximated using multipole expansion
        std::atomic_int approximatedNodes = { 0 };

        /// Number of opened leafs where exact (pair-wise) solution has been used
        std::atomic_int exactNodes = { 0 };
    };

    /// \brief Performs a recursive treewalk evaluating gravity for all particles.
    ///
    /// Treewalk starts at given node and subsequently calls \ref evalNode for children nodes. Function moves
    /// nodes from checkList into interaction lists and in case the node is a leaf, gravity is computed using
    /// constructed interaction lists.
    /// \param scheduler Scheduler used for (potential) parallelization
    /// \param dv Output thread-local buffer where computed accelerations are stored
    /// \param nodeIdx Index of the first node evaluated by the function
    /// \param data Checklist nad interaction lists of he node
    /// \param result Statistics incremented by the node. Cannot be return value, the type is not moveable.
    void evalNode(IScheduler& scheduler,
        ArrayView<Vector> dv,
        const Size nodeIdx,
        TreeWalkState data,
        TreeWalkResult& result) const;

    void evalParticleList(const LeafNode<BarnesHutNode>& leaf,
        ArrayView<Size> particleList,
        ArrayView<Vector> dv) const;

    void evalNodeList(const LeafNode<BarnesHutNode>& leaf,
        ArrayView<Size> nodeList,
        ArrayView<Vector> dv) const;

    Vector evalExact(const LeafNode<BarnesHutNode>& node, const Vector& r0, const Size idx) const;

    void buildLeaf(BarnesHutNode& node);

    void buildInner(BarnesHutNode& node, BarnesHutNode& left, BarnesHutNode& right);
};

NAMESPACE_SPH_END
