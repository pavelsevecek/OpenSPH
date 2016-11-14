#pragma once

/// Object defining computational domain.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Bounds.h"

NAMESPACE_SPH_BEGIN

/// \todo rename
enum class SubsetType {
    INSIDE,  ///< Marks all vectors inside of the domain
    OUTSIDE, ///< Marks all vectors outside of the domain
};

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

        /// Returns an array of indices, marking vectors with given property by their index
        virtual void getSubset(const ArrayView<Vector> vs,
                               Array<int>& output,
                               const SubsetType type) const = 0;

        /// Projects vectors outside of the domain onto its boundary. Vectors inside the domain are untouched.
        /// \param vs Array of vectors we want to project
        /// \param indices Optional array of indices. If passed, only selected vectors will be projected. All
        ///        vectors are projected by default.
        virtual void project(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const = 0;

        /// Inverts positions of vectors with a respect to the boundary. Projected vector does not have to be
        /// in the same distance to the boundary.
        /// \param vs Array of vectors we want to invert
        /// \param indices Optional array of indices. If passed, only selected vectors will be inverted. All
        ///        vectors are inverted by default.
        virtual void invert(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const = 0;

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

    /*virtual void areInside(const ArrayView<Vector> vs, ArrayView<bool> output) const override {
        ASSERT(vs.size() == output.size());
        for (int i = 0; i < vs.size(); ++i) {
            output[i] = isInsideImpl(vs[i]);
        }
    }*/
    virtual void getSubset(const ArrayView<Vector> vs,
                           Array<int>& output,
                           const SubsetType type) const override {
        switch (type) {
        case SubsetType::OUTSIDE:
            for (int i = 0; i < vs.size(); ++i) {
                if (!isInsideImpl(vs[i])) {
                    output.push(i);
                }
            }
            break;
        case SubsetType::INSIDE:
            for (int i = 0; i < vs.size(); ++i) {
                if (isInsideImpl(vs[i])) {
                    output.push(i);
                }
            }
            break;
        default:
            ASSERT(false);
        }
    }

    virtual void project(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const override {
        Float radius = Math::sqrt(radiusSqr);
        auto impl    = [this, radius](Vector& v) {
            if (!isInsideImpl(v)) {
                v = getNormalized(v - this->center) * radius + this->center;
            }
        };
        if (indices) {
            for (int i : indices) {
                impl(vs[i]);
            }
        } else {
            for (Vector& v : vs) {
                impl(v);
            }
        }
    }

    virtual void invert(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const override {
        Float radius = Math::sqrt(radiusSqr);
        if (indices) {
            for (int i : indices) {
                vs[i] = sphericalInversion(vs[i], this->center, radius);
            }
        } else {
            for (Vector& v : vs) {
                v = sphericalInversion(v, this->center, radius);
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

    /*virtual void areInside(const ArrayView<Vector> vs, ArrayView<bool> output) const override {
        ASSERT(vs.size() == output.size());
        for (int i = 0; i < vs.size(); ++i) {
            output[i] = box.contains(vs[i]);
        }
    }*/

    virtual void getSubset(const ArrayView<Vector> vs,
                           Array<int>& output,
                           const SubsetType type) const override {
        switch (type) {
        case SubsetType::OUTSIDE:
            for (int i = 0; i < vs.size(); ++i) {
                if (!box.contains(vs[i])) {
                    output.push(i);
                }
            }
            break;
        case SubsetType::INSIDE:
            for (int i = 0; i < vs.size(); ++i) {
                if (box.contains(vs[i])) {
                    output.push(i);
                }
            }
        }
    }

    virtual void project(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const override {
        auto impl = [this](Vector& v) {
            if (!box.contains(v)) {
                v = box.clamp(v);
            }
        };
        if (indices) {
            for (int i : indices) {
                impl(vs[i]);
            }
        } else {
            for (Vector& v : vs) {
                impl(v);
            }
        }
    }

    virtual void invert(ArrayView<Vector> UNUSED(vs),
                        ArrayView<int> UNUSED(indices) = nullptr) const override {
        /// \todo !!!
        ASSERT(false);
    }
};
/*
class InfiniteDomain : public Abstract::Domain {
public:
    InfiniteDomain()
        : Abstract::Domain(Vector(0._f), INFTY) {}

    virtual Float getVolume() const override {
        // formally return something, however this should never be called. Infinite domain cannot be used
in
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
