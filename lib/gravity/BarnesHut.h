#pragma once

#include "gravity/AbstractGravity.h"
#include "objects/finders/KdTree.h"

NAMESPACE_SPH_BEGIN

class BarnesHut : public Abstract::Gravity {
private:
    /// Source data
    ArrayView<const Vector> r;
    ArrayView<const Float> m;

    KdTree kdTree;

    /// Opening angle for multipole approximation (in radians)
    Float thetaSqr;


public:
    BarnesHut(const Float theta, const Size leafSize = 20)
        : kdTree(leafSize)
        , thetaSqr(sqr(theta)) {}

    /// Masses of particles must be strictly positive, otherwise center of mass would be undefined.
    virtual void build(ArrayView<const Vector> r, ArrayView<const Float> m) override;

    virtual Vector eval(const Size idx) override;

    virtual Vector eval(const Vector& r0) override;

private:
    Vector evalImpl(const Vector& r0, const Size idx);

    void buildLeaf(KdNode& node);

    void buildInner(KdNode& node, KdNode& left, KdNode& right);
};


NAMESPACE_SPH_END
