#pragma once

#include "gui/gui.h"
#include "gui/common.h"
#include "system/Callbacks.h"
#include "gui/glpane.h"

NAMESPACE_GUI_BEGIN

class GuiCallbacks : public Abstract::Callbacks {
private:
    CustomGlPane* glPane;

public:
    GuiCallbacks(CustomGlPane* glPane)
        : glPane(glPane) {}

    virtual void onTimeStep(ArrayView<const Vector> positions) override {
        /// \todo limit refreshing to some reasonable frame rate?
        glPane->draw(positions);
    }
};

NAMESPACE_GUI_END
