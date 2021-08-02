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

class RayMarcher : public IRaytracer {
private:
    /// BVH for finding intersections of rays with particles
    Bvh<BvhSphere> bvh;

    /// Finder for finding neighbors of intersected particles
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

        /// Cast shadows
        bool shadows = true;

        /// Render surface of spheres instead of an isosurface.
        bool renderSpheres = true;

    } fixed;

    struct MarchData {
        /// Neighbor indices of the current particle
        Array<Size> neighs;

        /// Intersection for the current ray
        Array<IntersectionInfo> intersections;

        /// Cached index of the previously evaluated particle, used for optimizations.
        Size previousIdx;

        MarchData() = default;
        MarchData(MarchData&& other) = default;
        MarchData(const MarchData& other)
            : neighs(other.neighs.clone())
            , intersections(other.intersections.clone())
            , previousIdx(other.previousIdx) {
            // needed to be used in Any, but never should be actually called
            SPH_ASSERT(false);
        }
    };

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

        /// Material ID for each particle
        Array<Size> materialIDs;

        /// Textures of the rendered bodies. Can be empty. The textures are assigned to the bodies using their
        /// material IDs.
        Array<SharedPtr<Texture>> textures;

        /// If true, the colors are used for emission, otherwise for diffuse reflectance.
        bool doEmission;

    } cached;

public:
    RayMarcher(SharedPtr<IScheduler> scheduler, const GuiSettings& settings);

    ~RayMarcher() override;

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    virtual bool isInitialized() const override;

private:
    virtual Rgba shade(const RenderParams& params,
        const CameraRay& cameraRay,
        ThreadData& data) const override;

    /// \param Creates a neighbor list for given particle.
    ///
    /// The neighbor list is cached and can be reused by the calling thread next time the function is called.
    /// \return View on the cached neighbor light.
    ArrayView<const Size> getNeighborList(MarchData& data, const Size index) const;

    /// \brief Returns the intersection of the iso-surface.
    ///
    /// If no intersection exists, function returns NOTHING.
    Optional<Vector> intersect(MarchData& data,
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
    Optional<Vector> getSurfaceHit(MarchData& data,
        const IntersectContext& context,
        const bool occlusion) const;

    /// \brief Returns the color of given hit point.
    Rgba getSurfaceColor(MarchData& data,
        const RenderParams& params,
        const Size index,
        const Vector& hit,
        const Vector& dir) const;

    Float evalField(ArrayView<const Size> neighs, const Vector& pos) const;

    Vector evalGradient(ArrayView<const Size> neighs, const Vector& pos) const;

    Rgba evalColor(ArrayView<const Size> neighs, const Vector& pos1) const;

    Vector evalUvws(ArrayView<const Size> neighs, const Vector& pos1) const;
};

NAMESPACE_SPH_END
