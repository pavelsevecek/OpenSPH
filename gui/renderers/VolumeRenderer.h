#pragma once

#include "gui/Settings.h"
#include "gui/objects/Color.h"
#include "gui/renderers/IRenderer.h"
#include "math/rng/Rng.h"
#include "objects/finders/Bvh.h"
#include "sph/kernel/Kernel.h"
#include "thread/ThreadLocal.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

class FrameBuffer;
class IColorMap;

class VolumeRenderer : public IRenderer {
private:
    /// BVH for finding intersections of rays with particles
    Bvh<BvhSphere> bvh;

    LutKernel<3> kernel;

    /// \brief Parameters fixed for the renderer.
    ///
    /// Note that additional parameters which can differ for each rendered image are passed to \ref render.
    struct {

        /// Number of iterations of the progressive renderer.
        Size iterationLimit;

        /// Color mapping operator
        AutoPtr<IColorMap> colorMap;

        /// Multiplier of the total emission
        float emission;

        struct {
            /// Environment color
            Rgba color;
        } enviro;

    } fixed;

    SharedPtr<IScheduler> scheduler;

    struct ThreadData {

        /// Intersection for the current ray
        std::set<IntersectionInfo> intersections;

        /// Random-number generator for this thread.
        UniformRng rng;

        ThreadData(int seed);
    };

    mutable ThreadLocal<ThreadData> threadData;

    struct {
        /// Particle positions
        Array<Vector> r;

        /// Particle colors
        Array<Rgba> colors;

    } cached;

    mutable std::atomic_bool shouldContinue;

public:
    VolumeRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings);

    ~VolumeRenderer();

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    virtual void render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const override;

    virtual void cancelRender() override {
        shouldContinue = false;
    }

private:
    void refine(const RenderParams& params, const Size iteration, FrameBuffer& fb) const;

    Rgba shade(const Ray& ray, ThreadData& data) const;
};

NAMESPACE_SPH_END
