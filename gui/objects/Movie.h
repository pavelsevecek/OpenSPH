#pragma once

/// \file Movie.h
/// \brief Periodically saves rendered images to disk
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "common/Globals.h"
#include "gui/renderers/IRenderer.h"
#include "io/Output.h"
#include <condition_variable>


NAMESPACE_SPH_BEGIN


class IRenderer;
enum class GuiSettingsId;
using GuiSettings = Settings<GuiSettingsId>;

/// \brief Object managing periodic rendering of images and saving them to given paths.
///
/// Rendered images are independend from the images in interactive window, i.e. they have dedicated renderer,
/// camera and list of colorizers to render.
class Movie : public Noncopyable {
private:
    /// Time step (framerate) of the movie
    Float outputStep;
    Float nextOutput;

    /// File names of the images
    OutputFile paths;

    /// Path (mask) of the final animations
    Path animationPath;

    /// Enable/disable image saving
    bool enabled;

    /// Renderer
    AutoPtr<IRenderer> renderer;

    /// Camera
    AutoPtr<ICamera> camera;

    /// Colorizers to render and save to disk
    Array<SharedPtr<IColorizer>> colorizers;

    RenderParams params;

    std::condition_variable waitVar;
    std::mutex waitMutex;

public:
    Movie(const GuiSettings& settings,
        AutoPtr<IRenderer>&& renderer,
        AutoPtr<ICamera>&& camera,
        Array<SharedPtr<IColorizer>>&& colorizers,
        const RenderParams& params);

    ~Movie();

    /// \brief Called every time step, saves the images every IMAGES_TIMESTEP.
    ///
    /// If the time since the last frame is less than the required framerate, function does nothing. If the
    /// run starts at negative time, no images are dumped till the time passes zero, consistently with Output
    /// implementations.
    ///
    /// Can be called from any thread; the function is blocking, waits until all images are saved.
    void onTimeStep(const Storage& storage, Statistics& stats);

    /// \brief Creates the animations from generated images.
    void finalize();

    void setEnabled(const bool enable = true);
};

NAMESPACE_SPH_END
