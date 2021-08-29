#pragma once

#include "gui/Settings.h"
#include "gui/objects/Color.h"
#include "gui/renderers/Brdf.h"
#include "gui/renderers/IRenderer.h"
#include "objects/finders/Bvh.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

class FrameBuffer;
class IBrdf;
class IColorMap;
class IShader;


/// \brief Base class for renderers based on raytracing
class Raytracer : public IRenderer {
private:
    SharedPtr<IScheduler> scheduler;

    /// \brief Parameters fixed for the renderer.
    ///
    /// Note that additional parameters which can differ for each rendered image are passed to \ref render.
    struct {

        struct {

            Rgba color = Rgba::black();

            /// HDRI for the background. Can be empty.
            Texture hdri;

        } enviro;

        /// Number of iterations of the progressive renderer.
        Size iterationLimit;

        /// Number of subsampled iterations.
        Size subsampling;

        /// Direction to sun; sun is assumed to be a point light source.
        Vector dirToSun;

        /// BRDF used to get the surface reflectance.
        // AutoPtr<IBrdf> brdf;

        float maxDistention = 50.f;

        /// Cast shadows
        bool shadows = true;

        /// Color mapping operator
        AutoPtr<IColorMap> colorMap;

    } fixed;

    struct {
        SharedPtr<IShader> surfaceness;
        SharedPtr<IShader> emission;
        SharedPtr<IShader> scattering;
        SharedPtr<IShader> absorption;
    } shaders;

    struct AttractorData {
        Vector position;
        float radius;
        SharedPtr<Texture> texture;
    };

    struct {
        /// Particle positions
        Array<Vector> r;

        /// Amount of surface. Value 1 means the surface is completely opaque.
        Array<float> surfaceness;

        /// Albedo of the surface. May be empty if there is no surface shader.
        Array<Rgba> albedo;

        /// Texture mapping coordinates. May be empty if there is no surface shader.
        Array<Vector> uvws;

        /// Emission. May be empty if there is no emission shader.
        Array<Rgba> emission;

        Array<Rgba> scattering;
        Array<Rgba> absorption;

        /// Particle volume (=mass/density)
        Array<float> v;

        /// Distention factor of each particle
        Array<float> distention;

        /// Particle indices
        Array<Size> flags;

        /// Material ID for each particle
        Array<Size> materialIDs;

        /// Textures of the rendered bodies. Can be empty. The textures are assigned to the bodies using their
        /// material IDs.
        Array<SharedPtr<Texture>> textures;

        Array<AttractorData> attractors;

    } cached;

    /// BVH for finding intersections of rays with particles
    Bvh<BvhSphere> bvh;

    /// Finder for finding neighbors of intersected particles
    AutoPtr<IBasicFinder> finder;

    LutKernel<3> kernel;

    struct ThreadData {
        /// Random-number generator for this thread.
        UniformRng rng;

        /// Neighbor indices of the current particle
        Array<Size> neighs;

        /// Intersection for the current ray
        Array<IntersectionInfo> intersections;

        /// Cached index of the previously evaluated particle, used for optimizations.
        Size previousIdx;

        ThreadData(int seed);
    };

    mutable ThreadLocal<ThreadData> threadData;

    mutable std::atomic_bool shouldContinue;

public:
    Raytracer(SharedPtr<IScheduler> scheduler, const GuiSettings& gui);

    ~Raytracer();

    void setSurfaceShader(const SharedPtr<IShader>& shader) {
        shaders.surfaceness = shader;
    }
    void setEmissionShader(const SharedPtr<IShader>& shader) {
        shaders.emission = shader;
    }
    void setAbsorptionShader(const SharedPtr<IShader>& shader) {
        shaders.absorption = shader;
    }
    void setScatteringShader(const SharedPtr<IShader>& shader) {
        shaders.scattering = shader;
    }

    virtual void initialize(const Storage& storage, const ICamera& camera) override;

    virtual bool isInitialized() const override;

    // virtual void setColorizer(const IColorizer& colorizer) override;

    virtual void render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const final;

    virtual void cancelRender() override {
        shouldContinue = false;
    }

private:
    void initializeVolumes(const Storage& storage);
    void initializeFlags(const Storage& storage);
    void initializeAttractors(const Storage& storage);
    void loadTextures(const Storage& storage);
    void evaluateShaders(const Storage& storage);
    void initializeStructures();

    void refine(const RenderParams& params, const Size iteration, FrameBuffer& fb) const;

    void postProcess(FrameBuffer& fb,
        const RenderParams& params,
        const bool isFinal,
        IRenderOutput& output) const;

    Rgba shade(const RenderParams& params, const CameraRay& ray, ThreadData& data) const;

    Rgba getEnviroColor(const CameraRay& ray) const;

    /// \param Creates a neighbor list for given particle.
    ///
    /// The neighbor list is cached and can be reused by the calling thread next time the function is called.
    /// \return View on the cached neighbor light.
    ArrayView<const Size> getNeighborList(ThreadData& data, const Size index) const;

    /// \brief Returns the intersection of the iso-surface.
    ///
    /// The intersections must be stored in \ref data. If no intersection exists, function returns NOTHING.
    Optional<Vector> intersect(ThreadData& data, const Ray& ray, const Float surfaceLevel) const;

    bool occluded(ThreadData& data, const Ray& ray, const Float surfaceLevel) const;

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

    bool isAttractor(const Size index) const {
        return index >= cached.r.size();
    }

    bool canBeSurfaceHit(const Size index) const;

    /// \brief Finds the actual surface point for given shade context.
    ///
    /// If no such point exists, function returns NOTHING.
    Optional<Vector> getSurfaceHit(ThreadData& data,
        const IntersectContext& context,
        const bool occlusion) const;

    /// \brief Returns the color of given hit point.
    Rgba getSurfaceColor(ThreadData& data,
        const RenderParams& params,
        const Size index,
        const Vector& hit,
        const Vector& dir) const;

    Rgba getAttractorColor(const RenderParams& params, const Size index, const Vector& hit) const;

    Rgba getVolumeColor(ThreadData& data,
        const RenderParams& params,
        const CameraRay& ray,
        const Rgba& baseColor,
        const float t_max) const;

    Float evalColorField(ArrayView<const Size> neighs, const Vector& pos) const;

    Vector evalNormal(ArrayView<const Size> neighs, const Vector& pos) const;

    template <typename T>
    T evalShader(ArrayView<const Size> neighs, const Vector& pos1, ArrayView<const T> data) const;

    // float evalSurfaceness(ArrayView<const Size> neighs, const Vector& pos1) const;

    Vector evalUvws(ArrayView<const Size> neighs, const Vector& pos1) const;
};

NAMESPACE_SPH_END
