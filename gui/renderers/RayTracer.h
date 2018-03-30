#pragma once

#include "gui/Settings.h"
#include "gui/objects/Color.h"
#include "gui/renderers/IRenderer.h"
#include "objects/finders/Bvh.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

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
        Float surfaceLevel = 0.5_f;

    } params;

    struct {
        /// Particle positions
        Array<Vector> r;

        /// Particle colors
        Array<Color> colors;

        /// Particle volume (=mass/density)
        Array<Float> v;

        /// Particle indices
        Array<Size> idxs;

        mutable Array<NeighbourRecord> neighs;

        mutable Size previousIdx;
    } cached;

public:
    RayTracer(const GuiSettings& settings);

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    virtual SharedPtr<Bitmap> render(const ICamera& camera,
        const RenderParams& params,
        Statistics& stats) const override;

private:
    Color shade(const Size i, const Ray& ray, const Float t_min) const;

    Float eval(const Vector& pos, const Size flag) const;
};

NAMESPACE_SPH_END
