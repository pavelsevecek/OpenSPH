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

        /// Returns an array of indices, marking vectors with given property by their index.
        /// \param output Output array, is not cleared by the method, previously stored values are kept
        /// unchanged.
        virtual void getSubset(const ArrayView<Vector> vs,
            Array<int>& output,
            const SubsetType type) const = 0;

        /// Returns distances of particles lying close to the boundary. The distances are signed, negative
        /// number means the particle is lying outside of the domain.
        /// \param vs Input array of partices.
        /// \param distances Output array, will be resized to the size of particle array and cleared.
        virtual void getDistanceToBoundary(ArrayView<Vector> vs, Array<Float>& distances) const = 0;

        /// Projects vectors outside of the domain onto its boundary. Vectors inside the domain are untouched.
        /// \param vs Array of vectors we want to project
        /// \param indices Optional array of indices. If passed, only selected vectors will be projected. All
        ///        vectors are projected by default.
        virtual void project(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const = 0;

        /// Inverts positions of vectors with a respect to the boundary. Projected vectors shall have the same
        /// distance to the boundary as the input vectors.
        /// \param vs Array of vectors we want to invert
        /// \param indices Optional array of indices. If passed, only selected vectors will be inverted. All
        ///        vectors are inverted by default.
        virtual void invert(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const = 0;

        /// \todo function for transforming block [0, 1]^d into the domain?
    };
}

/// Spherical domain, defined by the center of sphere and its radius.
class SphericalDomain : public Abstract::Domain {
private:
    Float radiusSqr;

public:
    SphericalDomain(const Vector& center, const Float& radius)
        : Abstract::Domain(center, radius)
        , radiusSqr(radius * radius) {}

    virtual Float getVolume() const override { return Math::sphereVolume(this->maxRadius); }

    virtual bool isInside(const Vector& v) const override { return isInsideImpl(v); }

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
            NOT_IMPLEMENTED;
        }
    }

    virtual void getDistanceToBoundary(ArrayView<Vector> vs, Array<Float>& distances) const override {
        distances.clear();
        Float radius = Math::sqrt(radiusSqr);
        for (const Vector& v : vs) {
            const Float dist = radius - getLength(v - this->center);
            distances.push(dist);
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
        auto impl    = [this, radius](Vector& v) {
            Float length;
            Vector normalized;
            tie(normalized, length) = getNormalizedWithLength(v - this->center);
            return v + 2._f * (radius - length) * normalized;
        };
        if (indices) {
            for (int i : indices) {
                vs[i] = impl(vs[i]);
            }
        } else {
            for (Vector& v : vs) {
                v = impl(v);
            }
        }
    }

private:
    INLINE bool isInsideImpl(const Vector& v) const { return getSqrLength(v - this->center) < radiusSqr; }
};


/// Block aligned with coordinate axes, defined by its center and length of each side.
class BlockDomain : public Abstract::Domain {
private:
    Box box;

public:
    BlockDomain(const Vector& center, const Vector& edges)
        : Abstract::Domain(center, 0.5_f * getLength(edges))
        , box(center - 0.5_f * edges, center + 0.5_f * edges) {}

    virtual Float getVolume() const override { return box.getVolume(); }

    virtual bool isInside(const Vector& v) const override { return box.contains(v); }

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

    virtual void getDistanceToBoundary(ArrayView<Vector> UNUSED(vs),
        Array<Float>& UNUSED(distances)) const override {
        NOT_IMPLEMENTED;
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
        NOT_IMPLEMENTED;
    }
};


/// Cylinder aligned with z-axis, optionally including bases (can be either open or close cylinder).
class CylindricalDomain : public Abstract::Domain {
private:
    Float radiusSqr; // radius of the base
    Float height;
    bool includeBases;

public:
    CylindricalDomain(const Vector& center, const Float radius, const Float height, const bool includeBases)
        : Abstract::Domain(center, Math::sqrt(Math::sqr(radius) + Math::sqr(height)))
        , radiusSqr(radius * radius)
        , includeBases(includeBases) {}

    virtual Float getVolume() const override { return Math::PI * radiusSqr * height; }

    virtual bool isInside(const Vector& v) const override { return this->isInsideImpl(v); }

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
        }
    }

    virtual void getDistanceToBoundary(ArrayView<Vector> vs, Array<Float>& distances) const override {
        distances.clear();
        Float radius = Math::sqrt(radiusSqr);
        ASSERT(!includeBases); // including bases not implemented
        for (const Vector& v : vs) {
            const Float dist = radius - getLength(Vector(v[X], v[Y], this->center[Z]) - this->center);
            distances.push(dist);
        }
    }

    virtual void project(ArrayView<Vector> vs, ArrayView<int> indices = nullptr) const override {
        Float radius = Math::sqrt(radiusSqr);
        auto impl    = [this, radius](Vector& v) {
            if (!isInsideImpl(v)) {
                v = getNormalized(Vector(v[X], v[Y], this->center[Z]) - this->center) * radius +
                    Vector(this->center[X], this->center[Y], v[Z]);
            }
        };
        ASSERT(!includeBases); // including bases not implemented
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
        auto impl    = [this, radius](Vector& v) {
            Float length;
            Vector normalized;
            tie(normalized, length) =
                getNormalizedWithLength(Vector(v[X], v[Y], this->center[Z]) - this->center);
            return v + 2._f * (radius - length) * normalized;
        };
        ASSERT(!includeBases); // including bases not implemented
        if (indices) {
            for (int i : indices) {
                vs[i] = impl(vs[i]);
            }
        } else {
            for (Vector& v : vs) {
                v = impl(v);
            }
        }
    }

private:
    INLINE bool isInsideImpl(const Vector& v) const {
        return getSqrLength(Vector(v[X], v[Y], this->center[Z]) - center) < radiusSqr &&
               Math::sqr(v[Z] - this->center[Z]) < Math::sqr(0.5_f * height);
    }
};


NAMESPACE_SPH_END
