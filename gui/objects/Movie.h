#pragma once

/// \file Movie.h
/// \brief Periodically saves rendered images to disk
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "common/Globals.h"
#include "gui/Settings.h"
#include "gui/renderers/IRenderer.h"
#include "io/Output.h"
#include "objects/containers/CallbackSet.h"

NAMESPACE_SPH_BEGIN

class IRenderer;
class ForwardingOutput;

class Movie : public Noncopyable {
private:
    AutoPtr<IRenderer> renderer;
    AutoPtr<IColorizer> colorizer;
    RenderParams params;
    int interpolatedFrames;

    OutputFile paths;

    Vector cameraVelocity;
    Float cameraOrbit;
    bool trackerMovesCamera;

    struct {
        Float time = 0._f;
        Storage data;
    } lastFrame;

public:
    Movie(const GuiSettings& settings,
        AutoPtr<IRenderer>&& renderer,
        AutoPtr<IColorizer>&& colorizer,
        RenderParams&& params,
        const int interpolatedFrames,
        const OutputFile& paths);

    ~Movie();

    void render(Storage&& storage, Statistics&& stats, IRenderOutput& output);

private:
    void renderImpl(const Storage& storage, Statistics& stats, ForwardingOutput& output);

    void updateCamera(const Storage& storage, const Float time);
};

Storage interpolate(const Storage& frame1, const Storage& frame2, const Float t);

NAMESPACE_SPH_END
