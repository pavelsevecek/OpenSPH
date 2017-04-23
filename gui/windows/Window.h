#pragma once

#include "gui/Settings.h"
#include "objects/wrappers/NonOwningPtr.h"
#include <wx/frame.h>

class wxComboBox;
class wxGauge;

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Renderer;
}
class Storage;

class Window : public wxFrame, public Observable {
private:
    std::shared_ptr<Storage> storage;
    Abstract::Renderer* renderer;
    wxComboBox* quantityBox;
    wxGauge* gauge;
    bool abortRun = false;

public:
    Window(const std::shared_ptr<Storage>& storage, const GuiSettings& guiSettings);

    Abstract::Renderer* getRenderer();

    bool shouldAbortRun() const {
        return abortRun;
    }

    void setProgress(const float progress);

private:
    void onComboBox(wxCommandEvent& evt);

    void onButton(wxCommandEvent& evt);
};

NAMESPACE_SPH_END
