#pragma once

#include "gui/GlPane.h"
#include "gui/Gui.h"
#include "gui/Renderer.h"
#include "objects/wrappers/NonOwningPtr.h"
#include "system/Callbacks.h"
#include "gui/Window.h"

NAMESPACE_SPH_BEGIN

class GuiCallbacks : public Abstract::Callbacks {
private:
    NonOwningPtr<Window> window;

public:
    GuiCallbacks(NonOwningPtr<Window> window)
        : window(window) {}

    virtual void onTimeStep(const float progress, const std::shared_ptr<Storage>& storage) override {
        /// \todo limit refreshing to some reasonable frame rate?
        if (window) {
            window->setProgress(progress);
            window->getRenderer()->draw(storage);
        }
    }

    virtual bool shouldAbortRun() const override {
        return window->shouldAbortRun();
    }
};

NAMESPACE_SPH_END
