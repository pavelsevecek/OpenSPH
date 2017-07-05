#pragma once

/// \file SurfaceRenderer.h
/// \brief Renderer visualizing the surface as triangle mesh
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Palette.h"
#include "gui/renderers/AbstractRenderer.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Camera;
    class Element;
}

class SurfaceRenderer : public Abstract::Renderer {
private:
    /// Parameters of Marching Cubes
    Float surfaceResolution;
    Float surfaceLevel;

    /// Cached values of visible particles, used for faster drawing.
    struct {
        /// Triangles of the surface
        Array<Point> triangles;

        /// Colors of particles assigned by the element
        Array<Color> colors;

    } cached;

public:
    SurfaceRenderer(const GuiSettings& settings);

    virtual void initialize(const Storage& storage,
        const Abstract::Element& element,
        const Abstract::Camera& camera);

    /// Can only be called from main thread
    virtual SharedPtr<Bitmap> render(const Abstract::Camera& camera,
        const RenderParams& params,
        Statistics& stats) const override;
};

NAMESPACE_SPH_END
