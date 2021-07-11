#pragma once

/// \file Domain.h
/// \brief Object defining computational domain.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "math/AffineMatrix.h"
#include "objects/geometry/Box.h"
#include "objects/wrappers/Optional.h"
#include "objects/wrappers/SharedPtr.h"

NAMESPACE_SPH_BEGIN

/// \todo rename
enum class SubsetType {
    INSIDE,  ///< Marks all vectors inside of the domain
    OUTSIDE, ///< Marks all vectors outside of the domain
};


struct Ghost {
    Vector position; ///< Position of the ghost
    Size index;      ///< Index into the original array of vectors
};


/// \brief Base class for computational domains.
class IDomain : public Polymorphic {
public:
    /// \brief Returns the center of the domain.
    virtual Vector getCenter() const = 0;

    /// \brief Returns the bounding box of the domain.
    virtual Box getBoundingBox() const = 0;

    /// \brief Returns the total volume of the domain.
    ///
    /// This should be identical to computing an integral of \ref isInside function, although faster and more
    /// precise.
    virtual Float getVolume() const = 0;

    /// \brief Returns the surface area of the domain.
    virtual Float getSurfaceArea() const = 0;

    /// \brief Checks if the given point lies inside the domain.
    ///
    /// Points lying exactly on the boundary of the domain are assumed to be inside.
    virtual bool contains(const Vector& v) const = 0;

    /// \brief Returns an array of indices, marking vectors with given property by their index.
    ///
    /// \param vs Input array of points.
    /// \param output Output array, is not cleared by the method, previously stored values are kept
    ///               unchanged. Indices of vectors belonging in the subset are pushed into the array.
    /// \param type Type of the subset, see \ref SubsetType.
    virtual void getSubset(ArrayView<const Vector> vs, Array<Size>& output, const SubsetType type) const = 0;

    /// \brief Returns distances of particles lying close to the boundary.
    ///
    /// The distances are signed, negative number means the particle is lying outside of the domain. Distances
    /// can be computed with small error to simplify implementation.
    /// \param vs Input array of points.
    /// \param distances Output array, will be resized to the size of particle array and cleared.
    ///
    /// \todo unify the (non)clearing of output arrays
    virtual void getDistanceToBoundary(ArrayView<const Vector> vs, Array<Float>& distances) const = 0;

    /// \brief Projects vectors outside of the domain onto its boundary.
    ///
    /// Vectors inside the domain are untouched. Function does not affect 4th component of vectors.
    /// \param vs Array of vectors we want to project
    /// \param indices Optional array of indices. If passed, only selected vectors will be projected. All
    ///        vectors are projected by default.
    virtual void project(ArrayView<Vector> vs, Optional<ArrayView<Size>> indices = NOTHING) const = 0;

    /// \brief Duplicates positions located close to the boundary, placing copies ("ghosts") symmetrically to
    /// the other side of the domain.
    ///
    /// Distance of the copy (ghost) to the boundary shall be the same as the source vector. One vector
    /// can create multiple ghosts.
    /// \param vs Array containing vectors creating ghosts.
    /// \param ghosts Output parameter containing created ghosts, stored as pairs (position of ghost and
    ///        index of source vector). Array must be cleared by the function!
    /// \param eta Dimensionless distance to the boundary necessary for creating a ghost. A ghost is
    ///        created for vector v if it is closer than radius * v[H]. Vector must be inside, outside
    ///        vectors are ignored.
    /// \param eps Minimal dimensionless distance of ghost from the source vector. When vector is too
    ///        close to the boundary, the ghost would be too close or even on top of the source vector;
    ///        implementation must place the ghost so that it is outside of the domain and at least
    ///        eps * v[H] from the vector. Must be strictly lower than radius, checked by assert.
    virtual void addGhosts(ArrayView<const Vector> vs,
        Array<Ghost>& ghosts,
        const Float eta = 2._f,
        const Float eps = 0.05_f) const = 0;

    /// \todo function for transforming block [0, 1]^d into the domain?
};


/// \brief Spherical domain, defined by the center of sphere and its radius.
class SphericalDomain : public IDomain {
private:
    Vector center;
    Float radius;

public:
    SphericalDomain(const Vector& center, const Float& radius);

    virtual Vector getCenter() const override;

    virtual Float getVolume() const override;

    virtual Float getSurfaceArea() const override;

    virtual Box getBoundingBox() const override;

    virtual bool contains(const Vector& v) const override;

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
        return getSqrLength(v - this->center) <= sqr(radius);
    }
};

/// \brief Axis aligned ellipsoidal domain, defined by the center of sphere and lengths of three axes.
class EllipsoidalDomain : public IDomain {
private:
    Vector center;

    /// Lengths of axes
    Vector radii;

    /// Effective radius (radius of a sphere with same volume)
    Float effectiveRadius;

public:
    EllipsoidalDomain(const Vector& center, const Vector& axes);

    virtual Vector getCenter() const override;

    virtual Float getVolume() const override;

    virtual Float getSurfaceArea() const override;

    virtual Box getBoundingBox() const override;

    virtual bool contains(const Vector& v) const override;

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
        return getSqrLength((v - center) / radii) <= 1._f;
    }
};


/// \brief Block aligned with coordinate axes, defined by its center and length of each side.
/// \todo create extra ghosts in the corners?
class BlockDomain : public IDomain {
private:
    Box box;

public:
    BlockDomain(const Vector& center, const Vector& edges);

    virtual Vector getCenter() const override;

    virtual Float getVolume() const override;

    virtual Float getSurfaceArea() const override;

    virtual Box getBoundingBox() const override;

    virtual bool contains(const Vector& v) const override;

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


/// \brief Cylinder aligned with z-axis, optionally including bases (can be either open or close cylinder).
class CylindricalDomain : public IDomain {
private:
    Vector center;
    Float radius;
    Float height;
    bool includeBases;

public:
    CylindricalDomain(const Vector& center, const Float radius, const Float height, const bool includeBases);

    virtual Vector getCenter() const override;

    virtual Float getVolume() const override;

    virtual Float getSurfaceArea() const override;

    virtual Box getBoundingBox() const override;

    virtual bool contains(const Vector& v) const override;

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
        return getSqrLength(Vector(v[X], v[Y], center[Z]) - center) <= sqr(radius) &&
               sqr(v[Z] - center[Z]) <= sqr(0.5_f * height);
    }
};


/// \brief Similar to cylindrical domain, but bases are hexagons instead of circles.
///
/// Hexagons are oriented so that two sides are parallel with x-axis.
/// \todo could be easily generalized to any polygon, currently not needed though
class HexagonalDomain : public IDomain {
private:
    Vector center;
    Float outerRadius; // bounding radius of the base
    Float innerRadius;
    Float height;
    bool includeBases;

public:
    HexagonalDomain(const Vector& center, const Float radius, const Float height, const bool includeBases);

    virtual Vector getCenter() const override;

    virtual Float getVolume() const override;

    virtual Float getSurfaceArea() const override;

    virtual Box getBoundingBox() const override;

    virtual bool contains(const Vector& v) const override;

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
        if (sqr(v[Z] - center[Z]) > sqr(0.5_f * height)) {
            return false;
        }
        const Float sqrLength = getSqrLength(v);
        if (sqrLength > sqr(outerRadius)) {
            return false;
        }
        if (sqrLength <= sqr(innerRadius)) {
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

/// See Muinonen 1998
class GaussianRandomSphere : public IDomain {
private:
    Vector center;
    Float a;
    Float beta;

public:
    GaussianRandomSphere(const Vector& center, const Float radius, const Float beta, const Size seed);

    virtual Vector getCenter() const override;

    virtual Float getVolume() const override;

    virtual Float getSurfaceArea() const override;

    virtual Box getBoundingBox() const override;

    virtual bool contains(const Vector& v) const override;

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
    Float sphericalHarmonic(const Float theta, const Float phi) const;
};

/// \brief Domain representing a half-space, given by z>0.
///
/// The domain has an infinite volume and thus cannot be used to generate particles. It is useful for
/// compositing with another domain or for specifying boundary conditions.
class HalfSpaceDomain : public IDomain {
public:
    virtual Vector getCenter() const override;

    virtual Float getVolume() const override;

    virtual Float getSurfaceArea() const override;

    virtual Box getBoundingBox() const override;

    virtual bool contains(const Vector& v) const override;

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
/// \brief Transform another domain by given transformation matrix
///
/// \todo TESTS
class TransformedDomain : public IDomain {
private:
    SharedPtr<IDomain> domain;
    AffineMatrix tm, tmInv;

public:
    TransformedDomain(SharedPtr<IDomain> domain, const AffineMatrix& matrix);

    virtual Vector getCenter() const override;

    virtual Float getVolume() const override;

    virtual Float getSurfaceArea() const override;

    virtual Box getBoundingBox() const override;

    virtual bool contains(const Vector& v) const override;

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
    Array<Vector> untransform(ArrayView<const Vector> vs) const;
};

NAMESPACE_SPH_END
