#pragma once

#include "gui/Settings.h"
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

        AutoPtr<IBrdf> brdf;
    } params;

    struct ThreadData {
        Array<NeighbourRecord> neighs;

        FlatSet<IntersectionInfo> intersections;

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

        /// How much distance is one pixel in world units
        Float pixelToWorldRatio;
    };

    Optional<Color> shade(ThreadData& data, const ShadeContext& context) const;

    Float evalField(ArrayView<const Size> neighs, const Vector& pos) const;

    Color evalColor(ArrayView<const Size> neighs, const Vector& pos1) const;
};

NAMESPACE_SPH_END
