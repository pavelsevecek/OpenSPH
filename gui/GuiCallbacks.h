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
    Movie movie;

public:
    GuiCallbacks(NonOwningPtr<Window> window, const GuiSettings& gui)
        : window(window)
        , movie(gui, window->getRenderer()) {}

    virtual void onTimeStep(const std::shared_ptr<Storage>& storage,
        const Statistics& stats,
        const Range timeRange) override {
        /// \todo limit refreshing to some reasonable frame rate?
        if (window) {
            executeOnMainThread([this, storage, stats, timeRange] {
                const float t = stats.get<Float>(StatisticsId::TOTAL_TIME);
                float progress = (t - timeRange.lower()) / timeRange.size();
                window->setProgress(progress);
                Abstract::Renderer* renderer = window->getRenderer();
                renderer->draw(storage, stats);
                movie.onTimeStep(t);
            });
        }
    }

    virtual void onRunEnd(const std::shared_ptr<Storage>& UNUSED(storage), const Statistics& stats) override {
        stats.get<Float>(StatisticsId::TOTAL_TIME);
    }

    virtual bool shouldAbortRun() const override {
        return window->shouldAbortRun();
    }
};

NAMESPACE_SPH_END
