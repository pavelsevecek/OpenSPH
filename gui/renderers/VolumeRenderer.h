#pragma once

#include "gui/objects/Color.h"
#include "gui/renderers/IRenderer.h"
#include "gui/renderers/Lensing.h"
#include "objects/finders/Bvh.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

class VolumeRenderer : public IRaytracer {
private:
    /// BVH for finding intersections of rays with particles
    Bvh<BvhSphere> bvh;

    struct {
        /// Particle positions
        Array<Vector> r;

        /// Particle colors
        Array<Rgba> colors;

        /// Mass-based radii
        Array<float> referenceRadii;

        /// Distention factor of each particle
        Array<float> distention;

        /// All attractors
        Array<AttractorData> attractors;

        /// Attractor textures
        Array<SharedPtr<Texture>> textures;

        /// Helper storage of textures, kept in memory between renders
        FlatMap<String, SharedPtr<Texture>> textureCache;

        /// Maximal distance for raymarching
        Float maxDistance;

    } cached;

    struct RayData {
        /// Current path
        LensingEffect::Segments segments;

        /// Intersection for the current path
        Array<CurvedRayIntersectionInfo> intersections;

        RayData() = default;
        RayData(RayData&& other) = default;
        RayData(const RayData& other)
            : intersections(other.intersections.clone()) {
            // needed to be used in Any, but never should be actually called
            SPH_ASSERT(false);
        }
    };

public:
    VolumeRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings);

    ~VolumeRenderer();

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    virtual bool isInitialized() const override;

    virtual void setColorizer(const IColorizer& colorizer) override;

private:
    virtual Rgba shade(const RenderParams& params,
        const CameraRay& cameraRay,
        ThreadData& data) const override;

    Rgba getAttractorColor(const RenderParams& params, const Size index, const Vector& hit) const;
};

NAMESPACE_SPH_END
