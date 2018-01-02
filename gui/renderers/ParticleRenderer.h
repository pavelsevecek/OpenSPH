#pragma once

/// \file ParticleRenderer.h
/// \brief Renderer drawing individual particles as dots
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Palette.h"
#include "gui/renderers/IRenderer.h"

NAMESPACE_SPH_BEGIN

class ParticleRenderer : public IRenderer {
private:
    /// Cutoff distance of visible particles.
    float cutoff;

    /// Grid size
    float grid;

    /// Cached values of visible particles, used for faster drawing.
    struct {
        /// Positions of particles
        Array<Vector> positions;

        /// Indices (in parent storage) of particles
        Array<Size> idxs;

        /// Colors of particles assigned by the colorizer
        Array<Color> colors;

        /// Color palette or NOTHING if no palette is drawn
        Optional<Palette> palette;

    } cached;

public:
    explicit ParticleRenderer(const GuiSettings& settings);

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    /// Can only be called from main thread
    virtual SharedPtr<Bitmap> render(const ICamera& camera,
        const RenderParams& params,
        Statistics& stats) const override;

    static void drawPalette(wxDC& dc, const Palette& palette);

    static void drawGrid(wxDC& dc, const ICamera& camera, const float grid);

private:
    bool isCutOff(const ICamera& camera, const Vector& r);
};

NAMESPACE_SPH_END
