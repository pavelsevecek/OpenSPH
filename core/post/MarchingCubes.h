#pragma once

#include "objects/containers/Array.h"
#include "objects/geometry/Box.h"
#include "objects/geometry/Triangle.h"
#include "objects/utility/Progressible.h"
#include "objects/wrappers/Function.h"

NAMESPACE_SPH_BEGIN

class Storage;
class IScheduler;

/// \brief Inferface for a generic scalar field, returning a float for given position.:w
class IScalarField : public Polymorphic {
public:
    /// Returns the value of the scalar field at given position
    virtual Float operator()(const Vector& pos) = 0;
};

/// \brief Marching cubes algorithm for generation of mesh from iso-surface of given scalar field.
class MarchingCubes : public Progressible<> {
private:
    IScheduler& scheduler;

    /// Input values
    Float surfaceLevel;

    /// Field, isosurface of which we want to triangularize
    SharedPtr<IScalarField> field;

    /// Output array of triangles
    Array<Triangle> triangles;

    /// Cached stuff to avoid re-allocation
    struct {
        /// Values of the scalar field defining the surface
        Array<Float> phi;
    } cached;

    class Cell;

public:
    /// \brief Constructs the object using given scalar field.
    ///
    /// \param scheduler Scheduler used for parallelization
    /// \param surfaceLevel Defines of the boundary of SPH particle as implicit function \f$ {\rm Boundary} =
    ///                     \Phi(\vec r) - {\rm surfaceLevel}\f$, where \f$\Phi\f$ is the scalar field.
    /// \param field Scalar field used to generate the surface.
    MarchingCubes(IScheduler& scheduler, const Float surfaceLevel, const SharedPtr<IScalarField>& field);

    /// \brief Adds a triangle mesh representing the boundary of particles.
    ///
    /// Particles are specified by the given bounding box. The generated mesh is added into the internal
    /// triangle buffer.
    /// \param box Selected bounding box
    /// \param gridResolution Absolute size of the grid
    void addComponent(const Box& box, const Float gridResolution);

    /// Returns the generated triangles.
    INLINE Array<Triangle>& getTriangles() & {
        return triangles;
    }

    /// \copydoc getTriangles
    INLINE Array<Triangle> getTriangles() && {
        return std::move(triangles);
    }

private:
    /// Add triangles representing isolevel of the field in given cell into the interal triangle buffer
    /// \param cell Cell used in intersection
    /// \param tri Output triangle array, may contain previously generated triangles.
    INLINE void intersectCell(Cell& cell, Array<Triangle>& tri);

    /// Find the interpolated vertex position based on the surface level
    Vector interpolate(const Vector& v1, const Float p1, const Vector& v2, const Float p2) const;

    /// \brief Parallelized version of \ref Box::iterateWithIndices
    ///
    /// Returns false if user cancelled the operation.
    template <typename TFunctor>
    bool iterateWithIndices(const Box& box, const Vector& step, TFunctor&& functor);
};

struct McConfig {
    /// Absolute size of each produced triangle.
    Float gridResolution = 1.e2_f;

    /// (Number) density defining the surface. Higher value is more likely to cause SPH particles being
    /// separated into smaller groups (droplets), lower value will cause the boundary to be "bulgy" rather
    /// than smooth.
    Float surfaceLevel = 0.12_f;

    /// Multiplier of the smoothing lengths
    Float smoothingMult = 1._f;

    /// If true, anisotropic kernels of Yu & Turk (2010) are used instead of normal isotropic kernels.
    bool useAnisotropicKernels = false;

    /// Generic functor called during MC evaluation
    Function<bool(Float progress)> progressCallback = nullptr;
};

/// \brief Returns the triangle mesh of the body surface (or surfaces of bodies).
///
/// \param scheduler Scheduler used for parallelization.
/// \param storage Particle storage; must contain particle positions.
/// \return Array of generated triangles. Can be empty if no boundary exists.
Array<Triangle> getSurfaceMesh(IScheduler& scheduler, const Storage& storage, const McConfig& config);


NAMESPACE_SPH_END
