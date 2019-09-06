#pragma once

/// \file ContourRenderer.h
/// \brief Renderer drawing iso-contours of specified quantity
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "gui/objects/Palette.h"
#include "gui/renderers/IRenderer.h"
#include "objects/wrappers/SharedPtr.h"
#include "sph/kernel/Kernel.h"

NAMESPACE_SPH_BEGIN

class IScheduler;
class IBasicFinder;

class ContourRenderer : public IRenderer {
private:
    SharedPtr<IScheduler> scheduler;

    AutoPtr<IBasicFinder> finder;

    LutKernel<3> kernel;

    struct {
        Array<Vector> positions;

        Array<float> values;

        Optional<Palette> palette;

    } cached;

public:
    ContourRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings);

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    virtual void render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const override;

    virtual void cancelRender() override {}
};

NAMESPACE_SPH_END
