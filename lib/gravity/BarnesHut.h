#pragma once

#include "gravity/AbstractGravity.h"
#include "objects/finders/KdTree.h"
#include "sph/kernel/GravityKernel.h"

NAMESPACE_SPH_BEGIN

enum class MultipoleOrder {
    MONOPOLE = 0,
    QUADRUPOLE = 2,
    OCTUPOLE = 3,
    HEXADECAPOLE = 4,
};


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

    /// Opening angle for multipole approximation (in radians)
    Float thetaSqr;

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
    virtual void build(ArrayView<const Vector> r, ArrayView<const Float> m) override;

    virtual Vector eval(const Size idx) override;

    virtual Vector eval(const Vector& r0) override;

    /// Returns the multipole moments computed from root node.
    MultipoleExpansion<3> getMoments() const;

protected:
    virtual Vector evalImpl(const Vector& r0, const Size idx);

    virtual void buildLeaf(KdNode& node);

    virtual void buildInner(KdNode& node, KdNode& left, KdNode& right);
};


NAMESPACE_SPH_END
