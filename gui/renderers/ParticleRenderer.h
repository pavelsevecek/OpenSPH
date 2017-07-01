#pragma once

/// \file ParticleRenderer.h
/// \brief Renderer drawing individual particles as dots
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/objects/Bitmap.h"
#include "gui/objects/Palette.h"
#include "gui/renderers/AbstractRenderer.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Camera;
    class Element;
}


class ParticleRenderer : public Abstract::Renderer {
private:
    /// Cached values of visible particles, used for faster drawing.
    struct {
        /// Positions of particles
        Array<Vector> positions;

        /// Indices (in parent storage) of particles
        Array<Size> idxs;

        /// Colors of particles assigned by the element
        Array<Color> colors;

        /// Color palette or NOTHING if no palette is drawn
        Optional<Palette> palette;

    } cached;

public:
    virtual void initialize(const Storage& storage,
        const Abstract::Element& element,
        const Abstract::Camera& camera);

    /// Can only be called from main thread
    virtual SharedPtr<Bitmap> render(const Abstract::Camera& camera,
        const RenderParams& params,
        Statistics& stats) const override;

    void drawPalette(wxDC& dc, const Palette& palette) const;
};

NAMESPACE_SPH_END
