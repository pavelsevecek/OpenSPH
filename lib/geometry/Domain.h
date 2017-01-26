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
        /// \todo currently not used, remove?
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
    SphericalDomain(const Vector& center, const Float& radius);

    virtual Float getVolume() const override;

    virtual bool isInside(const Vector& v) const override;

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override;

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override;

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override;

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override;

private:
    INLINE bool isInsideImpl(const Vector& v) const { return getSqrLength(v - this->center) < radiusSqr; }
};


/// Block aligned with coordinate axes, defined by its center and length of each side.
/// \todo create extra ghosts in the corners?
class BlockDomain : public Abstract::Domain {
private:
    Box box;

public:
    BlockDomain(const Vector& center, const Vector& edges);

    virtual Float getVolume() const override;

    virtual bool isInside(const Vector& v) const override;

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override;

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override;

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override;

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override;
};


/// Cylinder aligned with z-axis, optionally including bases (can be either open or close cylinder).
class CylindricalDomain : public Abstract::Domain {
private:
    Float radiusSqr; // radius of the base
    Float height;
    bool includeBases;

public:
    CylindricalDomain(const Vector& center, const Float radius, const Float height, const bool includeBases);

    virtual Float getVolume() const override;

    virtual bool isInside(const Vector& v) const override;

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override;

    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const override;

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override;

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override;

private:
    INLINE bool isInsideImpl(const Vector& v) const {
        return getSqrLength(Vector(v[X], v[Y], this->center[Z]) - center) <= radiusSqr &&
               sqr(v[Z] - this->center[Z]) <= sqr(0.5_f * height);
    }
};


/// Similar to cylindrical domain, but bases are hexagons instead of circles. Hexagons are oriented so that
/// two sides are parallel with x-axis.
/// \todo could be easily generalized to any polygon, currently not needed though
class HexagonalDomain : public Abstract::Domain {
private:
    Float outerRadiusSqr; // bounding radius of the base
    Float innerRadiusSqr;
    Float height;
    bool includeBases;

public:
    HexagonalDomain(const Vector& center, const Float radius, const Float height, const bool includeBases);

    virtual Float getVolume() const override;

    virtual bool isInside(const Vector& v) const override;

    virtual void getSubset(ArrayView<const Vector> vs,
        Array<Size>& output,
        const SubsetType type) const override;

    virtual void getDistanceToBoundary(ArrayView<const Vector>, Array<Float>&) const override;

    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const override;

    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta,
        const Float eps) const override;

private:
    INLINE bool isInsideImpl(const Vector& v) const {
        if (sqr(v[Z] - this->center[Z]) > sqr(0.5_f * height)) {
            return false;
        }
        const Float sqrLength = getSqrLength(v);
        if (sqrLength > outerRadiusSqr) {
            return false;
        }
        if (sqrLength <= innerRadiusSqr) {
            return true;
        }
        const Float phi = atan2(v[Y], v[X]);
        return getSqrLength(Vector(v[X], v[Y], 0._f)) <= sqr(hexagon(phi));
    }

    /// Polar plot of hexagon
    INLINE Float hexagon(const Float phi) const {
        return 0.5_f * SQRT_3 * 1._f / sin(phi - PI / 3._f * (floor(phi / (PI / 3._f)) - 1._f));
    }
};


NAMESPACE_SPH_END
