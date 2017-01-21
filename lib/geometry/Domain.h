#pragma once

/// Object defining computational domain.
/// Pavel Sevecek 2016
/// sevecek at sirrah.troja.mff.cuni.cz

#include "geometry/Box.h"
#include "objects/wrappers/Optional.h"

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
        virtual void getSubset(ArrayView<const Vector> vs,
            Array<Size>& output,
            const SubsetType type) const = 0;

        /// Returns distances of particles lying close to the boundary. The distances are signed, negative
        /// number means the particle is lying outside of the domain.
        /// \param vs Input array of partices.
        /// \param distances Output array, will be resized to the size of particle array and cleared.
        virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const = 0;

        /// Projects vectors outside of the domain onto its boundary. Vectors inside the domain are untouched.
        /// Function does not affect 4th component of vectors.
        /// \param vs Array of vectors we want to project
        /// \param indices Optional array of indices. If passed, only selected vectors will be projected. All
        ///        vectors are projected by default.
        virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const = 0;

        struct Ghost {
            Vector position; ///< Position of the ghost
            Size index;      ///< Index into the original array of vectors
        };

        /// Duplicates vectors located close to the boundary, placing the symmetrically to the other side.
        /// Distance of the copy (ghost) to the boundary shall be the same as the source vector. One vector
        /// can create multiple ghosts.
        /// \param vs Array containing vectors creating ghosts.
        /// \param ghosts Output parameter containing created ghosts, stored as pairs (position of ghost and
        ///        index of source vector). Array must be cleared by the function!
        /// \param radius Dimensionless distance to the boundary necessary for creating a ghost. A ghost is
        ///        created for vector v if it is closer than radius * v[H]. Vector must be inside, outside
        ///        vectors are ignored.
        /// \param eps Minimal dimensionless distance of ghost from the source vector. When vector is too
        ///        close to the boundary, the ghost would be too close or even on top of the source vector;
        ///        implementation must place the ghost so that it is outside of the domain and at least
        ///        eps * v[H] from the vector. Must be strictly lower than radius, checked by assert.
        virtual void addGhosts(ArrayView<const Vector> vs,
            Array<Ghost>& ghosts,
            const Float radius = 2._f,
            const Float eps = 0.05_f) const = 0;

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

    virtual Float getVolume() const override { return sphereVolume(this->maxRadius); }

    virtual bool isInside(const Vector& v) const override { return isInsideImpl(v); }

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override {
        switch (type) {
        case SubsetType::OUTSIDE:
            for (Size i = 0; i < vs.size(); ++i) {
                if (!isInsideImpl(vs[i])) {
                    output.push(i);
                }
            }
            break;
        case SubsetType::INSIDE:
            for (Size i = 0; i < vs.size(); ++i) {
                if (isInsideImpl(vs[i])) {
                    output.push(i);
                }
            }
            break;
        default:
            NOT_IMPLEMENTED;
        }
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override {
        distances.clear();
        Float radius = sqrt(radiusSqr);
        for (const Vector& v : vs) {
            const Float dist = radius - getLength(v - this->center);
            distances.push(dist);
        }
    }

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override {
        Float radius = sqrt(radiusSqr);
        auto impl = [this, radius](Vector& v) {
            if (!isInsideImpl(v)) {
                const Float h = v[H];
                v = getNormalized(v - this->center) * radius + this->center;
                v[H] = h;
            }
        };
        if (indices) {
            for (Size i : indices.get()) {
                impl(vs[i]);
            }
        } else {
            for (Vector& v : vs) {
                impl(v);
            }
        }
    }

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override {
        ASSERT(eps < eta);
        ghosts.clear();
        const Float radius = sqrt(radiusSqr);
        // iterate using indices as the array can reallocate during the loop
        for (Size i = 0; i < vs.size(); ++i) {
            if (!isInsideImpl(vs[i])) {
                continue;
            }
            Float length;
            Vector normalized;
            tieToTuple(normalized, length) = getNormalizedWithLength(vs[i] - this->center);
            const Float h = vs[i][H];
            const Float diff = radius - length;
            if (diff < h * eta) {
                Vector v = vs[i] + max(eps * h, 2._f * diff) * normalized;
                v[H] = h;
                ghosts.push(Ghost{ v, i });
            }
        }
    }

private:
    INLINE bool isInsideImpl(const Vector& v) const { return getSqrLength(v - this->center) < radiusSqr; }
};


/// Block aligned with coordinate axes, defined by its center and length of each side.
/// \todo create extra ghosts in the corners?
class BlockDomain : public Abstract::Domain {
private:
    Box box;

public:
    BlockDomain(const Vector& center, const Vector& edges)
        : Abstract::Domain(center, 0.5_f * getLength(edges))
        , box(center - 0.5_f * edges, center + 0.5_f * edges) {}

    virtual Float getVolume() const override { return box.getVolume(); }

    virtual bool isInside(const Vector& v) const override { return box.contains(v); }

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override {
        switch (type) {
        case SubsetType::OUTSIDE:
            for (Size i = 0; i < vs.size(); ++i) {
                if (!box.contains(vs[i])) {
                    output.push(i);
                }
            }
            break;
        case SubsetType::INSIDE:
            for (Size i = 0; i < vs.size(); ++i) {
                if (box.contains(vs[i])) {
                    output.push(i);
                }
            }
        }
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override {
        distances.clear();
        for (const Vector& v : vs) {
            const Vector d1 = v - box.lower();
            const Vector d2 = box.upper() - v;
            // we cannot just select min element of abs, we need signed distance
            Float minDist = INFTY, minAbsDist = INFTY;
            for (int i = 0; i < 3; ++i) {
                Float dist = abs(d1[i]);
                if (dist < minAbsDist) {
                    minAbsDist = dist;
                    minDist = d1[i];
                }
                dist = abs(d2[i]);
                if (dist < minAbsDist) {
                    minAbsDist = dist;
                    minDist = d2[i];
                }
            }
            ASSERT(minDist < INFTY);
            distances.push(minDist);
        }
    }

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override {
        auto impl = [this](Vector& v) {
            if (!box.contains(v)) {
                const Float h = v[H];
                v = box.clamp(v);
                v[H] = h;
            }
        };
        if (indices) {
            for (Size i : indices.get()) {
                impl(vs[i]);
            }
        } else {
            for (Vector& v : vs) {
                impl(v);
            }
        }
    }

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override {
        ASSERT(eps < eta);
        ghosts.clear();
        for (Size i = 0; i < vs.size(); ++i) {
            if (!box.contains(vs[i])) {
                continue;
            }
            const Float h = vs[i][H];
            const Vector d1 = max(vs[i] - box.lower(), Vector(eps * h));
            const Vector d2 = max(box.upper() - vs[i], Vector(eps * h));
            // each face for the box can potentially create a ghost
            if (d1[X] < eta * h) {
                Vector v = vs[i] - Vector(2._f * d1[X], 0._f, 0._f);
                v[H] = h;
                ghosts.push(Ghost{ v, i });
            }
            if (d1[Y] < eta * h) {
                Vector v = vs[i] - Vector(0._f, 2._f * d1[Y], 0._f);
                v[H] = h;
                ghosts.push(Ghost{ v, i });
            }
            if (d1[Z] < eta * h) {
                Vector v = vs[i] - Vector(0._f, 0._f, 2._f * d1[Z]);
                v[H] = h;
                ghosts.push(Ghost{ v, i });
            }

            if (d2[X] < eta * h) {
                Vector v = vs[i] + Vector(2._f * d2[X], 0._f, 0._f);
                v[H] = h;
                ghosts.push(Ghost{ v, i });
            }
            if (d2[Y] < eta * h) {
                Vector v = vs[i] + Vector(0._f, 2._f * d2[Y], 0._f);
                v[H] = h;
                ghosts.push(Ghost{ v, i });
            }
            if (d2[Z] < eta * h) {
                Vector v = vs[i] + Vector(0._f, 0._f, 2._f * d2[Z]);
                v[H] = h;
                ghosts.push(Ghost{ v, i });
            }
        }
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
        : Abstract::Domain(center, sqrt(sqr(radius) + sqr(height)))
        , radiusSqr(radius * radius)
        , height(height)
        , includeBases(includeBases) {}

    virtual Float getVolume() const override { return PI * radiusSqr * height; }

    virtual bool isInside(const Vector& v) const override { return this->isInsideImpl(v); }

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override {
        switch (type) {
        case SubsetType::OUTSIDE:
            for (Size i = 0; i < vs.size(); ++i) {
                if (!isInsideImpl(vs[i])) {
                    output.push(i);
                }
            }
            break;
        case SubsetType::INSIDE:
            for (Size i = 0; i < vs.size(); ++i) {
                if (isInsideImpl(vs[i])) {
                    output.push(i);
                }
            }
        }
    }

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override {
        distances.clear();
        Float radius = sqrt(radiusSqr);
        for (const Vector& v : vs) {
            const Float dist = radius - getLength(Vector(v[X], v[Y], this->center[Z]) - this->center);
            if (includeBases) {
                /// \todo properly implement includeBases
                distances.push(min(dist, abs(0.5_f * height - (v[Z] - this->center[Z]))));
            } else {
                distances.push(dist);
            }
        }
    }

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override {
        Float radius = sqrt(radiusSqr);
        auto impl = [this, radius](Vector& v) {
            if (!isInsideImpl(v)) {
                const Float h = v[H];
                v = getNormalized(Vector(v[X], v[Y], this->center[Z]) - this->center) * radius +
                    Vector(this->center[X], this->center[Y], v[Z]);
                v[H] = h;
            }
        };
        ASSERT(!includeBases); // including bases not implemented
        if (indices) {
            for (Size i : indices.get()) {
                impl(vs[i]);
            }
        } else {
            for (Vector& v : vs) {
                impl(v);
            }
        }
    }

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override {
        ghosts.clear();
        ASSERT(eps < eta);
        Float radius = sqrt(radiusSqr);
        for (Size i = 0; i < vs.size(); ++i) {
            if (!isInsideImpl(vs[i])) {
                continue;
            }
            Float length;
            Vector normalized;
            tieToTuple(normalized, length) =
                getNormalizedWithLength(Vector(vs[i][X], vs[i][Y], this->center[Z]) - this->center);
            const Float h = vs[i][H];
            ASSERT(radius - length >= 0._f);
            Float diff = max(eps * h, radius - length);
            if (diff < h * eta) {
                Vector v = vs[i] + 2._f * diff * normalized;
                v[H] = h;
                ghosts.push(Ghost{ v, i });
            }
            if (includeBases) {
                diff = 0.5_f * height - (vs[i][Z] - this->center[Z]);
                if (diff < h * eta) {
                    ghosts.push(Ghost{ vs[i] + Vector(0._f, 0._f, 2._f * diff), i });
                }
                diff = 0.5_f * height - (this->center[Z] - vs[i][Z]);
                if (diff < h * eta) {
                    ghosts.push(Ghost{ vs[i] - Vector(0._f, 0._f, 2._f * diff), i });
                }
            }
        }
    }

private:
    INLINE bool isInsideImpl(const Vector& v) const {
        return getSqrLength(Vector(v[X], v[Y], this->center[Z]) - center) < radiusSqr &&
               sqr(v[Z] - this->center[Z]) < sqr(0.5_f * height);
    }
};


NAMESPACE_SPH_END
