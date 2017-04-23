#include "gui/Gui.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/problems/Collision.h"
#include "gui/problems/Meteoroid.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/OrthoPane.h"
#include "gui/windows/Window.h"
#include "io/Logger.h"
#include "io/Output.h"
#include "physics/Constants.h"
#include "run/Run.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"
#include "timestepping/TimeStepping.h"

#include <wx/glcanvas.h>
#include <wx/sizer.h>
#include <wx/wx.h>


IMPLEMENT_APP(Sph::MyApp)

NAMESPACE_SPH_BEGIN


bool MyApp::OnInit() {
    run = std::make_unique<AsteroidCollision>();
    worker = std::thread([this]() { run->run(); });

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
