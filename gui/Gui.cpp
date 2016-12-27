#include "gui/Gui.h"
#include "gui/Callbacks.h"
#include "gui/GlPane.h"
#include "gui/OrthoPane.h"
#include "gui/Settings.h"
#include "gui/Window.h"
#include "physics/Constants.h"
#include "problem/Problem.h"
#include "sph/initial/Initial.h"
#include "system/Factory.h"

#include <wx/glcanvas.h>
#include <wx/sizer.h>
#include <wx/wx.h>


IMPLEMENT_APP(Sph::MyApp)

NAMESPACE_SPH_BEGIN

void MyApp::OnButton(wxCommandEvent& evt) {
    evt.Skip();
}

bool MyApp::OnInit() {
    window = new Window();
    window->SetAutoLayout(true);
    window->Show();

    auto globalSettings = GLOBAL_SETTINGS;
    Problem* p          = new Problem(globalSettings);
    p->logger           = std::make_unique<StdOutLogger>();
    p->timeRange        = Range(0._f, 1000._f);
    p->timeStepping     = Factory::getTimestepping(globalSettings, p->storage);

    auto bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::ENERGY, 0.001_f);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 1000);
    InitialConditions conds(p->storage, globalSettings);
    SphericalDomain domain1(Vector(0._f), 1._f);
    conds.addBody(domain1, bodySettings);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    SphericalDomain domain2(Vector(2._f, 1._f, 0._f), 0.3_f);
    conds.addBody(domain2, bodySettings);

    p->callbacks = std::make_unique<GuiCallbacks>(window->getRenderer());
    worker       = std::thread([&p]() { p->run(); });

    return true;
}

MyApp::~MyApp() {
    worker.join();
}

NAMESPACE_SPH_END
