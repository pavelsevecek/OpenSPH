#pragma once

#include "gui/MainLoop.h"
#include "objects/Object.h"
#include "wx/wx.h"

class wxFrame;

NAMESPACE_SPH_BEGIN

class Controller;

class App : public wxApp {
private:
    SharedPtr<Controller> model;

public:
    App();
    ~App();

private:
    virtual bool OnInit() override;

    void processEvents(MainLoopEvent& evt);
};

NAMESPACE_SPH_END
