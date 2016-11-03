#pragma once

/// Object defining computational domain.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Bounds.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    /// Base class for computational domains.
    class Domain : public Polymorphic {
    protected:
        Vector center;
        Float maxRadius;

    public:
        /// Constructs the domain given its center point and a radius of a sphere containing the whole domain
        Domain(const Vector& center, const Float& maxRadius)
            : center(center)
            , maxRadius(maxRadius) {
            ASSERT(maxRadius > 0._f);
        }

        /// Returns the center of the domain
        virtual Vector getCenter() const { return this->center; }

        /// Returns the bounding radius of the domain
        virtual Float getBoundingRadius() const { return this->maxRadius; }

        /// Returns the total d-dimensional volume of the domain
        virtual Float getVolume() const = 0;

        /// Checks if the vector lies inside the domain
        virtual bool isInside(const Vector& v) const = 0;

        /// Checks if elements of an array are inside the domain. Returns the result as array of bools. Array
        /// must be already allocated and its size must match array vs.
        virtual void areInside(const ArrayView<Vector> vs, ArrayView<bool> output) const = 0;

        /// Projects vectors outside of the domain onto its boundary. Vectors inside the domains are untouched.
        virtual void project(ArrayView<Vector> vs) const = 0;


        /// \todo function for transforming block [0, 1]^d into the domain?
    };
}

class SphericalDomain : public Abstract::Domain {
private:
    Float radiusSqr;

    INLINE bool isInsideImpl(const Vector& v) const { return getSqrLength(v - this->center) < radiusSqr; }

public:
    SphericalDomain(const Vector& center, const Float& radius)
        : Abstract::Domain(center, radius)
        , radiusSqr(radius * radius) {}

    virtual Float getVolume() const override { return Math::sphereVolume(this->maxRadius); }

    virtual bool isInside(const Vector& v) const override { return isInsideImpl(v); }

    virtual void areInside(const ArrayView<Vector> vs, ArrayView<bool> output) const override {
        ASSERT(vs.size() == output.size());
        for (int i = 0; i < vs.size(); ++i) {
            output[i] = isInsideImpl(vs[i]);
        }
    }

    virtual void project(ArrayView<Vector> vs) const override {
        Float radius = Math::sqrt(radiusSqr);
        for (Vector& v : vs) {
            if (!isInsideImpl(v)) {
                v = getNormalized(v - this->center) * radius + this->center;
            }
        }
    }
};

class BlockDomain : public Abstract::Domain {
private:
    Box box;

public:
    BlockDomain(const Vector& center, const Vector& edges)
        : Abstract::Domain(center, 0.5_f * getLength(edges))
        , box(center - 0.5_f * edges, center + 0.5_f * edges) {}

    virtual Float getVolume() const override { return box.getVolume(); }

    virtual bool isInside(const Vector& v) const override { return box.contains(v); }

    virtual void areInside(const ArrayView<Vector> vs, ArrayView<bool> output) const override {
        ASSERT(vs.size() == output.size());
        for (int i = 0; i < vs.size(); ++i) {
            output[i] = box.contains(vs[i]);
        }
    }

    virtual void project(ArrayView<Vector> vs) const override {
        for (Vector& v : vs) {
            if (!box.contains(v)) {
                v = box.clamp(v);
            }
        }
    }
};
/*
class InfiniteDomain : public Abstract::Domain {
public:
    InfiniteDomain()
        : Abstract::Domain(Vector(0._f), INFTY) {}

    virtual Float getVolume() const override {
        // formally return something, however this should never be called. Infinite domain cannot be used in
        // situations where the volume is relevant (generating of particle positions, ...)
        ASSERT(false);
        return INFTY;
    }

    virtual bool isInside(const Vector& UNUSED(v)) const override { return true; }

    virtual void areInside(const ArrayView<Vector> vs, ArrayView<bool> output) const override {
        for (int i = 0; i < vs.getSize(); ++i) {
            output[i] = true;
        }
    }

     virtual void project(ArrayView<Vector> UNUSED(vs)) const override {
        // nothing to do here ...
    }
};*/

NAMESPACE_SPH_END
