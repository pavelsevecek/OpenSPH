#pragma once

#include "gravity/AbstractGravity.h"
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

    virtual Vector eval(const Vector& r0, Statistics& stats) const override;

    /// Returns the multipole moments computed from root node.
    MultipoleExpansion<3> getMoments() const;

protected:
    Vector evalImpl(const Vector& r0, const Size idx, Statistics& stats) const;

    Vector evalExact(const LeafNode& node, const Vector& r0, const Size idx) const;

    void buildLeaf(KdNode& node);

    void buildInner(KdNode& node, KdNode& left, KdNode& right);
};


NAMESPACE_SPH_END
