#pragma once

#include "gui/Gui.h"
#include "gui/Renderer.h"
#include "gui/objects/Movie.h"
#include "gui/windows/Window.h"
#include "objects/wrappers/NonOwningPtr.h"
#include "system/Callbacks.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

class GuiCallbacks : public Abstract::Callbacks {
private:
    NonOwningPtr<Window> window;
    Range timeRange;
    Movie movie;

public:
    GuiCallbacks(NonOwningPtr<Window> window, const GlobalSettings& settings, const GuiSettings& gui)
        : window(window)
        , timeRange(settings.get<Range>(GlobalSettingsIds::RUN_TIME_RANGE))
        , movie(gui, window->getRenderer()) {}

    virtual void onTimeStep(const std::shared_ptr<Storage>& storage, const Statistics& stats) override {
        /// \todo limit refreshing to some reasonable frame rate?
        if (window) {
            /// \todo this is still not thread safe, window can be destroyed while executing these function.
            const float t = stats.get<Float>(StatisticsIds::TOTAL_TIME);
            float progress = (t - timeRange.lower()) / timeRange.size();
            window->setProgress(progress);
            Abstract::Renderer* renderer = window->getRenderer();
            renderer->draw(storage, stats);
            movie.onTimeStep(t);
        }
    }

    virtual bool shouldAbortRun() const override {
        return window->shouldAbortRun();
    }
};

NAMESPACE_SPH_END
