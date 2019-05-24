#pragma once

/// \file ParticleRenderer.h
/// \brief Renderer drawing individual particles as dots
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Palette.h"
#include "gui/renderers/IRenderer.h"
#include "system/Timer.h"
#include <atomic>

NAMESPACE_SPH_BEGIN

class IRenderContext;

/// \todo exposed for PaletteDialog, should be possibly generalized, used by other renderers, etc.
void drawPalette(IRenderContext& context,
    const Pixel origin,
    const Pixel size,
    const Rgba& lineColor,
    const Palette& palette);

class ParticleRenderer : public IRenderer {
private:
    /// Grid size
    float grid;

    /// Show ghost particles
    bool renderGhosts;

    /// Cached values of visible particles, used for faster drawing.
    struct {
        /// Positions of particles
        Array<Vector> positions;

        /// Indices (in parent storage) of particles
        Array<Size> idxs;

        /// Colors of particles assigned by the colorizer
        Array<Rgba> colors;

        /// Vectors representing the colorized quantity. May be empty.
        Array<Vector> vectors;

        /// Color palette or NOTHING if no palette is drawn
        Optional<Palette> palette;

        /// Camera direction for which the cached values have been sorted.
        Vector cameraDir;

    } cached;

    mutable std::atomic_bool shouldContinue;

    mutable Timer lastRenderTimer;

public:
    explicit ParticleRenderer(IScheduler& scheduler, const GuiSettings& settings);

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    virtual void render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const override;

    virtual void cancelRender() override;
};

NAMESPACE_SPH_END
