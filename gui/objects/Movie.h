#pragma once

/// \file Movie.h
/// \brief Periodically saves rendered images to disk
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "common/Globals.h"
#include "gui/Renderer.h"
#include "io/Output.h"
#include <condition_variable>
#include <memory>

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Renderer;
    class Element;
}
enum class GuiSettingsId;
using GuiSettings = Settings<GuiSettingsId>;

class Movie : public std::enable_shared_from_this<Movie> {
private:
    /// time step (framerate) of the movie
    Float outputStep;
    Float nextOutput;

    /// file names of the images
    OutputFile paths;

    /// enable/disable image saving
    bool enabled;

    /// renderer
    std::unique_ptr<Abstract::Renderer> renderer;
    RenderParams params;

    /// elements to rende1r and save to disk
    Array<std::unique_ptr<Abstract::Element>> elements;

    std::condition_variable waitVar;
    std::mutex waitMutex;

public:
    Movie(const GuiSettings& settings,
        std::unique_ptr<Abstract::Renderer>&& renderer,
        const RenderParams& params,
        Array<std::unique_ptr<Abstract::Element>>&& elements);

    ~Movie();

    /// Called every time step, saves the images every IMAGES_TIMESTEP. If the time since the last frame is
    /// less than the required framerate, function does nothing. Can be called from any thread; the function
    /// is blocking, waits until all images are saved.
    void onTimeStep(const std::shared_ptr<Storage>& storage, Statistics& stats);

    void setEnabled(const bool enable = true);
};

NAMESPACE_SPH_END
