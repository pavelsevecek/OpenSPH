#pragma once

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class App : public wxApp {
private:
    AutoPtr<Controller> controller;

public:
    App() = default;

    ~App() {
        controller->quit();
    }

private:
    virtual bool OnInit() override;

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
