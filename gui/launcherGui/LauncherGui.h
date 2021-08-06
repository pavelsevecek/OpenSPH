#pragma once

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include <wx/app.h>

NAMESPACE_SPH_BEGIN

class MainWindow;

class App : public wxApp {
private:
    MainWindow* window;

public:
    App() = default;


private:
    virtual bool OnInit() override;

    virtual int OnExit() override;

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
