#pragma once

/// \file Movie.h
/// \brief Periodically saves rendered images to disk
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Globals.h"
#include "gui/renderers/IRenderer.h"
#include "io/Output.h"
#include <condition_variable>


NAMESPACE_SPH_BEGIN


class IRenderer;
class IElement;
enum class GuiSettingsId;
using GuiSettings = Settings<GuiSettingsId>;

/// \brief Object managing periodic rendering of images and saving them to given paths.
///
/// Rendered images are independend from the images in interactive window, i.e. they have dedicated renderer,
/// camera and list of elements to render.
class Movie : public Noncopyable {
private:
    /// time step (framerate) of the movie
    Float outputStep;
    Float nextOutput;

    /// file names of the images
    OutputFile paths;

    /// enable/disable image saving
    bool enabled;

    /// renderer
    AutoPtr<IRenderer> renderer;

    /// camera
    AutoPtr<ICamera> camera;

    /// elements to rende1r and save to disk
    Array<SharedPtr<IColorizer>> elements;

    RenderParams params;

    std::condition_variable waitVar;
    std::mutex waitMutex;

public:
    Movie(const GuiSettings& settings,
        AutoPtr<IRenderer>&& renderer,
        AutoPtr<ICamera>&& camera,
        Array<SharedPtr<IColorizer>>&& elements,
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

    void setEnabled(const bool enable = true);
};

NAMESPACE_SPH_END
