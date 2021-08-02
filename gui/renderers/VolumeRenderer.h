#pragma once

#include "gui/objects/Color.h"
#include "gui/renderers/IRenderer.h"
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

        /// Distention factor of each particle
        Array<float> distention;

    } cached;

    struct RayData {
        /// Intersection for the current ray
        Array<IntersectionInfo> intersections;

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

private:
    virtual Rgba shade(const RenderParams& params,
        const CameraRay& cameraRay,
        ThreadData& data) const override;
};

NAMESPACE_SPH_END
