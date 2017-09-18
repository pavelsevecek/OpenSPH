#pragma once

/// \file SurfaceRenderer.h
/// \brief Renderer visualizing the surface as triangle mesh
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Palette.h"
#include "gui/renderers/IRenderer.h"
#include "post/MarchingCubes.h"

NAMESPACE_SPH_BEGIN

class SurfaceRenderer : public IRenderer {
private:
    /// Parameters of Marching Cubes
    Float surfaceResolution;
    Float surfaceLevel;

    /// Shading parameters
    Vector sunPosition;
    Float sunIntensity;
    Float ambient;

    /// Cached values of visible particles, used for faster drawing.
    struct {
        /// Triangles of the surface
        Array<Triangle> triangles;

        /// Colors of particles assigned by the colorizer
        Array<Color> colors;

    } cached;

public:
    SurfaceRenderer(const GuiSettings& settings);

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    /// Can only be called from main thread
    virtual SharedPtr<Bitmap> render(const ICamera& camera,
        const RenderParams& params,
        Statistics& stats) const override;
};

NAMESPACE_SPH_END
