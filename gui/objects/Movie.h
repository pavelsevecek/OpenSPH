#pragma once

/// Periodically saves rendered images to disk
/// Pavel Sevecek
/// sevecek at sirrah.troja.mff.cuni.cz

#include "core/Globals.h"
#include "gui/MainLoop.h"
#include "gui/Renderer.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "system/Output.h"
#include <wx/image.h>

NAMESPACE_SPH_BEGIN

class Movie {
private:
    Float outputStep;
    Float nextOutput;
    bool enabled;
    OutputFile paths;
    Abstract::Renderer* renderer;

public:
    Movie(const GuiSettings& settings, Abstract::Renderer* renderer)
        : outputStep(settings.get<Float>(GuiSettingsIds::IMAGES_TIMESTEP))
        , renderer(renderer) {
        enabled = settings.get<bool>(GuiSettingsIds::IMAGES_SAVE);
        const std::string directory = settings.get<std::string>(GuiSettingsIds::IMAGES_PATH);
        const std::string name = settings.get<std::string>(GuiSettingsIds::IMAGES_NAME);
        paths = OutputFile(directory + name);
        static bool first = true;
        if (first) {
            wxInitAllImageHandlers();
            first = false;
        }
        nextOutput = outputStep;
    }

    // gui callbacks?
    void onTimeStep(const Float time) {
        if (time < nextOutput || !enabled) {
            return;
        }
        Bitmap bitmap = renderer->getRender();
        executeOnMainThread([this, bitmap] { bitmap.saveToFile(paths.getNextPath()); });
        nextOutput += outputStep;
    }

    void setEnabled(const bool enable = true) {
        enabled = enable;
    }
};

NAMESPACE_SPH_END
