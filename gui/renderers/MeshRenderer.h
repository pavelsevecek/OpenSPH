#pragma once

/// \file MeshRenderer.h
/// \brief Renderer visualizing the surface as triangle mesh
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2019

#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Palette.h"
#include "gui/renderers/IRenderer.h"
#include "post/MarchingCubes.h"
#include "sph/kernel/Interpolation.h"

NAMESPACE_SPH_BEGIN

class MeshRenderer : public IRenderer {
private:
    /// Parameters of Marching Cubes
    Float surfaceResolution;
    Float surfaceLevel;

    /// Shading parameters
    Vector sunPosition;
    float sunIntensity;
    float ambient;

    /// Cached values of visible particles, used for faster drawing.
    struct {
        /// Triangles of the surface
        Array<Triangle> triangles;

        /// Colors of surface vertices assigned by the colorizer
        Array<Rgba> colors;

    } cached;

    SharedPtr<IScheduler> scheduler;

    /// Finder used for colorization of the surface
    AutoPtr<IBasicFinder> finder;

    LutKernel<3> kernel;

public:
    MeshRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings);

    virtual void initialize(const Storage& storage,
        const IColorizer& colorizer,
        const ICamera& camera) override;

    /// Can only be called from main thread
    virtual void render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const override;

    virtual void cancelRender() override {}
};

NAMESPACE_SPH_END
