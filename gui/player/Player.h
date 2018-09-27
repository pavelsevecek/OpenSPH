#pragma once

/// \file Player.h
/// \brief Visualization of previously saved frames (as .ssf files)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/Controller.h"
#include "gui/MainLoop.h"
#include "io/Output.h"
#include "run/IRun.h"
#include <wx/app.h>
#include <wx/filedlg.h>

NAMESPACE_SPH_BEGIN

class RunPlayer : public IRun {
private:
    RawPtr<Controller> controller;

    Path fileMask;

    OutputFile files;
    Size fileCnt;

    Float fps = 100._f;

    Float loadedTime;

public:
    RunPlayer(const Path& fileMask);

    void setController(RawPtr<Controller> newController) {
        controller = newController;
    }

    virtual void setUp() override;

    virtual void run() override;


protected:
    virtual void tearDown(const Statistics& stats) override;

    Size getFileCount(const Path& pathMask) const;
};

class App : public wxApp {
private:
    AutoPtr<Controller> controller;

public:
    App() = default;

    ~App() {
        if (controller) {
            controller->quit();
        }
    }

private:
    virtual bool OnInit() override;

    void processEvents(MainLoopEvent& evt) {
        evt.execute();
    }
};

NAMESPACE_SPH_END
