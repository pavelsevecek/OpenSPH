#include "gui/Gui.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/problems/Collision.h"
#include "gui/problems/Meteoroid.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/Window.h"
#include "physics/Constants.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "sph/timestepping/TimeStepping.h"
#include "system/Factory.h"
#include "system/Logger.h"
#include "system/Output.h"

#include <wx/glcanvas.h>
#include <wx/sizer.h>
#include <wx/wx.h>


IMPLEMENT_APP(Sph::MyApp)

NAMESPACE_SPH_BEGIN


bool MyApp::OnInit() {
    // MeteoroidEntry setup;
    AsteroidCollision setup;

    p = setup.getProblem();
    GuiSettings guiSettings = setup.getGuiSettings();
    window = new Window(p->storage, guiSettings, [this, setup]() mutable {
        ASSERT(this->worker.joinable());
        this->worker.join();
        p->storage->removeAll();
        setup.initialConditions(p->storage);
        this->worker = std::thread([this]() { p->run(); });
    });
    window->SetAutoLayout(true);
    window->Show();

    p->callbacks = std::make_unique<GuiCallbacks>(window, setup.globalSettings, guiSettings);
    worker = std::thread([this]() { p->run(); });

    Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(MyApp::processEvents));
    return true;
}

void MyApp::processEvents(MainLoopEvent& evt) {
    evt.execute();
    evt.Skip();
}

MyApp::MyApp() {}

MyApp::~MyApp() {
    worker.join();
}

NAMESPACE_SPH_END
