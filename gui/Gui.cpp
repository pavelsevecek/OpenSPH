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
#include "gui/Settings.h"

#include <wx/glcanvas.h>
#include <wx/sizer.h>
#include <wx/wx.h>


IMPLEMENT_APP(Sph::MyApp)

NAMESPACE_SPH_BEGIN

void MyApp::OnButton(wxCommandEvent& evt) {
    evt.Skip();
}

bool MyApp::OnInit() {
    auto globalSettings = GLOBAL_SETTINGS;
    globalSettings.set(GlobalSettingsIds::DOMAIN_BOUNDARY, BoundaryEnum::GHOST_PARTICLES);
    globalSettings.set(GlobalSettingsIds::DOMAIN_RADIUS, 2.5_f);
    globalSettings.set(GlobalSettingsIds::DOMAIN_TYPE, DomainEnum::SPHERICAL);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_ADAPTIVE, true);
    globalSettings.set(GlobalSettingsIds::TIMESTEPPING_INITIAL_TIMESTEP, 1.e-8_f);
    globalSettings.set(GlobalSettingsIds::MODEL_FORCE_DIV_S, false);
    /*globalSettings.set(GlobalSettingsIds::MODEL_DAMAGE, DamageEnum::SCALAR_GRADY_KIPP);
    globalSettings.set(GlobalSettingsIds::MODEL_YIELDING, YieldingEnum::VON_MISES);*/
    Problem* p          = new Problem(globalSettings);
    p->logger           = std::make_unique<StdOutLogger>();
    p->timeRange        = Range(0._f, 1000._f);
    p->timeStepping     = Factory::getTimestepping(globalSettings, p->storage);

    auto bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::ENERGY, 1.e-6_f);
    bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 10000);
    bodySettings.set(BodySettingsIds::EOS, EosEnum::TILLOTSON);
    InitialConditions conds(p->storage, globalSettings);
    SphericalDomain domain1(Vector(0._f), 1._f);
    conds.addBody(domain1, bodySettings, Vector(5e5_f, 0._f, 0._f));
    //bodySettings.set(BodySettingsIds::PARTICLE_COUNT, 100);
    //SphericalDomain domain2(Vector(2._f, 1._f, 0._f), 0.3_f);
    //conds.addBody(domain2, bodySettings, Vector(-5._f, 0._f, 0._f));

    GuiSettings guiSettings = GUI_SETTINGS;
    guiSettings.set<Float>(GuiSettingsIds::VIEW_FOV, 2._f);
    guiSettings.set<Float>(GuiSettingsIds::PARTICLE_RADIUS, 0.5_f);
    window = new Window(p->storage, guiSettings);
    window->SetAutoLayout(true);
    window->Show();

    p->callbacks = std::make_unique<GuiCallbacks>(window->getRenderer());
    worker       = std::thread([&p]() { p->run(); });
    return true;
}

MyApp::~MyApp() {
    worker.join();
}

NAMESPACE_SPH_END
