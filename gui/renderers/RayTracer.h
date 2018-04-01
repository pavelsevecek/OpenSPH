#pragma once

#include "gui/Settings.h"
#include "gui/Uvw.h"
#include "gui/objects/Color.h"
#include "gui/renderers/Brdf.h"
#include "gui/renderers/IRenderer.h"
#include "objects/finders/Bvh.h"
#include "sph/kernel/Kernel.h"
#include "thread/ThreadLocal.h"

NAMESPACE_SPH_BEGIN

class IBrdf;

class RayTracer : public IRenderer {
private:
    /// BVH for finding intersections of rays with particles
    Bvh<BvhSphere> bvh;

    /// Finds for finding neighbours of intersected particles
    /// \todo we need to share finders! Right now we can have finder in SPH, gravity, density sum colorizer
    /// and here
    AutoPtr<IBasicFinder> finder;

    LutKernel<3> kernel;

    struct {
        /// Iso-level of the surface; see GuiSettingsId::SURFACE_LEVEL.
        Float surfaceLevel;

        /// Direction to sun; sun is assumed to be a point light source.
        Vector dirToSun;

        /// Ambient light illuminating every point unconditionally.
        Float ambientLight;

        /// BRDF used to get the surface reflectance.
        AutoPtr<IBrdf> brdf;

        /// HDRI for the background. Can be empty.
        Texture hdri;

        /// Textures of the rendered bodies. Can be empty. The textures are assigned to the bodies using their
        /// flags (not material IDs).
        Array<Texture> textures;

        /// Cast shadows
        bool shadows = true;

    } params;

    struct ThreadData {
        Array<Size> neighs;

        std::set<IntersectionInfo> intersections;

        Size previousIdx;
    };

    /// Thread pool for parallelization; we need to use a custom instance instead of the global one as there
    /// is currently no way to wait for just some tasks - using the global instance could clash with the
    /// simulation tasks,
    mutable ThreadPool pool;

    mutable ThreadLocal<ThreadData> threadData;

    struct {
        /// Particle positions
        Array<Vector> r;

        /// Particle colors
        Array<Color> colors;

        Array<Vector> uvws;

        /// Particle volume (=mass/density)
        Array<Float> v;

        /// Particle indices
        Array<Size> flags;

    } cached;

public:
    explicit RayTracer(const GuiSettings& settings);

    ~RayTracer();

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    virtual SharedPtr<wxBitmap> render(const ICamera& camera,
        const RenderParams& params,
        Statistics& stats) const override;

private:
    struct ShadeContext {
        /// Particle hit by the ray
        Size index;

        /// Ray casted from the camera
        Ray ray;

        /// Distance of the sphere hit, i.e. the minimap distance of the actual hit.
        Float t_min;
    };

    /// \param Creates a neighbour list for given particle.
    ///
    /// The neighbour list is cached and can be reused by the calling thread next time the function is called.
    /// \return View on the cached neighbour light.
    ArrayView<const Size> getNeighbourList(ThreadData& data, const Size index) const;

    /// \brief Returns the intersection of the iso-surface.
    ///
    /// If no intersection exists, function returns NOTHING.
    Optional<Vector> intersect(ThreadData& data, const Ray& ray, const bool occlusion) const;

    /// \brief Finds the actual surface point for given shade context.
    ///
    /// If no such point exists, function returns NOTHING.
    Optional<Vector> getSurfaceHit(ThreadData& data, const ShadeContext& context, const bool occlusion) const;

    /// \brief Returns the color of given hit point.
    Color shade(ThreadData& data, const Size index, const Vector& hit, const Vector& dir) const;

    Color getEnviroColor(const Ray& ray) const;

    Float evalField(ArrayView<const Size> neighs, const Vector& pos) const;

    Vector evalGradient(ArrayView<const Size> neighs, const Vector& pos) const;

    Color evalColor(ArrayView<const Size> neighs, const Vector& pos1) const;

    Vector evalUvws(ArrayView<const Size> neighs, const Vector& pos1) const;
};

NAMESPACE_SPH_END
