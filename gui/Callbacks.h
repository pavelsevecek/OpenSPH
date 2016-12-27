#pragma once

#include "gui/GlPane.h"
#include "gui/Gui.h"
#include "gui/Renderer.h"
#include "objects/wrappers/NonOwningPtr.h"
#include "system/Callbacks.h"

NAMESPACE_SPH_BEGIN

class GuiCallbacks : public Abstract::Callbacks {
private:
    NonOwningPtr<Abstract::Renderer> renderer;

public:
    GuiCallbacks(NonOwningPtr<Abstract::Renderer> renderer)
        : renderer(renderer) {}

    virtual void onTimeStep(const std::shared_ptr<Storage>& storage) override {
        /// \todo limit refreshing to some reasonable frame rate?
        if (renderer) {
            renderer->draw(storage);
        }
    }
};

NAMESPACE_SPH_END
