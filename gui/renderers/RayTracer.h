#pragma once

#include "gui/Settings.h"
#include "gui/objects/Color.h"
#include "gui/objects/Texture.h"
#include "gui/renderers/Brdf.h"
#include "gui/renderers/IRenderer.h"
#include "objects/finders/Bvh.h"
#include "sph/kernel/Kernel.h"
#include "thread/ThreadLocal.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

class FrameBuffer;
class Seeder;
class IBrdf;

class RayTracer : public IRenderer {
private:
    /// BVH for finding intersections of rays with particles
    Bvh<BvhSphere> bvh;

    /// Finder for finding neighbours of intersected particles
    AutoPtr<IBasicFinder> finder;

    LutKernel<3> kernel;

    /// \brief Parameters fixed for the renderer.
    ///
    /// Note that additional parameters which can differ for each rendered image are passed to \ref render.
    struct {

        /// Direction to sun; sun is assumed to be a point light source.
        Vector dirToSun;

        /// BRDF used to get the surface reflectance.
        AutoPtr<IBrdf> brdf;

        struct {

            Rgba color = Rgba::black();

            /// HDRI for the background. Can be empty.
            Texture hdri;

        } enviro;

        /// Textures of the rendered bodies. Can be empty. The textures are assigned to the bodies using their
        /// flags (not material IDs).
        Array<Texture> textures;

        /// Cast shadows
        bool shadows = false;

        /// Number of iterations of the progressive renderer.
        Size iterationLimit;

        /// Number of subsampled iterations.
        Size subsampling;

    } fixed;

    SharedPtr<IScheduler> scheduler;

    struct ThreadData {
        /// Neighbour indices of the current particle
        Array<Size> neighs;

        /// Intersection for the current ray
        std::set<IntersectionInfo> intersections;

        /// Cached index of the previously evaluated particle, used for optimizations.
        Size previousIdx;

        /// Random-number generator for this thread.
        UniformRng rng;

        ThreadData(Seeder& seeder);
    };

    mutable ThreadLocal<ThreadData> threadData;

    struct {
        /// Particle positions
        Array<Vector> r;

        /// Particle colors
        Array<Rgba> colors;

        /// Mapping coordinates. May be empty.
        Array<Vector> uvws;

        /// Particle volume (=mass/density)
        Array<Float> v;

        /// Particle indices
        Array<Size> flags;

        /// If true, the colors are used for emission, otherwise for diffuse reflectance.
        bool doEmission;

    } cached;

    mutable std::atomic_bool shouldContinue;

public:
    RayTracer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings);

    ~RayTracer();

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    virtual void render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const override;

    virtual void cancelRender() override {
        shouldContinue = false;
    }

private:
    void refine(const RenderParams& params, const Size iteration, FrameBuffer& fb) const;

    /// \param Creates a neighbour list for given particle.
    ///
    /// The neighbour list is cached and can be reused by the calling thread next time the function is called.
    /// \return View on the cached neighbour light.
    ArrayView<const Size> getNeighbourList(ThreadData& data, const Size index) const;

    /// \brief Returns the intersection of the iso-surface.
    ///
    /// If no intersection exists, function returns NOTHING.
    Optional<Vector> intersect(ThreadData& data,
        const Ray& ray,
        const Float surfaceLevel,
        const bool occlusion) const;

    struct IntersectContext {
        /// Particle hit by the ray.
        Size index;

        /// Ray casted from the camera.
        Ray ray;

        /// Distance of the sphere hit, i.e. the minimap distance of the actual hit.
        Float t_min;

        /// Selected value of the iso-surface.
        Float surfaceLevel;
    };

    /// \brief Finds the actual surface point for given shade context.
    ///
    /// If no such point exists, function returns NOTHING.
    Optional<Vector> getSurfaceHit(ThreadData& data,
        const IntersectContext& context,
        const bool occlusion) const;

    /// \brief Returns the color of given hit point.
    Rgba shade(ThreadData& data,
        const RenderParams& params,
        const Size index,
        const Vector& hit,
        const Vector& dir) const;

    Rgba getEnviroColor(const Ray& ray) const;

    Float evalField(ArrayView<const Size> neighs, const Vector& pos) const;

    Vector evalGradient(ArrayView<const Size> neighs, const Vector& pos) const;

    Rgba evalColor(ArrayView<const Size> neighs, const Vector& pos1) const;

    Vector evalUvws(ArrayView<const Size> neighs, const Vector& pos1) const;
};

NAMESPACE_SPH_END
