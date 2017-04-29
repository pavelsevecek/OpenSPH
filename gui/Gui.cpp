#include "gui/Gui.h"
#include "geometry/Domain.h"
#include "gui/GuiCallbacks.h"
#include "gui/Settings.h"
#include "gui/problems/Collision.h"
#include "gui/problems/Meteoroid.h"
#include "gui/windows/GlPane.h"
#include "gui/windows/MainWindow.h"
#include "gui/windows/OrthoPane.h"
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


IMPLEMENT_APP(Sph::App)

NAMESPACE_SPH_BEGIN

bool App::OnInit() {
    model = std::make_shared<Controller>();

    // connect event handler
    Connect(MAIN_LOOP_TYPE, MainLoopEventHandler(App::processEvents));
    return true;
}

void App::processEvents(MainLoopEvent& evt) {
    evt.execute();
    evt.Skip();
}

App::App() {}

App::~App() {
    model->quit();
}

NAMESPACE_SPH_END
