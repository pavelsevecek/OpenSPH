#pragma once

#include "gui/Settings.h"
#include "gui/objects/Color.h"
#include "gui/renderers/IRenderer.h"
#include "math/rng/Rng.h"
#include "objects/finders/Bvh.h"
#include "sph/kernel/Kernel.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

class VolumeRenderer : public IRaytracer {
private:
    /// BVH for finding intersections of rays with particles
    Bvh<BvhSphere> bvh;

    LutKernel<3> kernel;

    struct {
        /// Particle positions
        Array<Vector> r;

        /// Particle colors
        Array<Rgba> colors;

        /// Distention factor of each particle
        Array<float> distention;

    } cached;

public:
    VolumeRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings);

    ~VolumeRenderer();

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

private:
    virtual Rgba shade(const RenderParams& params,
        const CameraRay& cameraRay,
        ThreadData& data) const override;
};

NAMESPACE_SPH_END
