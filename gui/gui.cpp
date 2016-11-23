#include "gui/gui.h"
#include "gui/callbacks.h"
#include "gui/glpane.h"
#include "wx/glcanvas.h"
#include "wx/sizer.h"
#include "wx/wx.h"

#include "physics/Constants.h"
#include "problem/Problem.h"
#include "system/Factory.h"


IMPLEMENT_APP(MyApp)

using namespace Sph;
using namespace Sph::Gui;

enum class ButtonIds { START };

void MyApp::OnButton(wxCommandEvent& evt) {
    evt.Skip();
}

bool MyApp::OnInit() {
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    frame = new wxFrame((wxFrame*)NULL, -1, wxT("Hello GL World"), wxPoint(50, 50), wxSize(400, 200));

    int args[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0 };

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxButton(frame, int(ButtonIds::START), "START"));
    Connect(wxEVT_BUTTON, wxCommandEventHandler(MyApp::OnButton), nullptr, frame);
    buttonSizer->Add(new wxButton(frame, wxID_ANY, "button2"));
    sizer->Add(buttonSizer);

    glPane = new CustomGlPane((wxFrame*)frame, args);
    sizer->Add(glPane, 1, wxEXPAND);

    frame->SetSizer(sizer);
    frame->SetAutoLayout(true);

    frame->Show();

    auto globalSettings = GLOBAL_SETTINGS;
    globalSettings.set<int>(GlobalSettingsIds::SPH_FINDER, int(FinderEnum::BRUTE_FORCE));
    Problem<ContinuitySolver<3>>* p = new Problem<ContinuitySolver<3>>(globalSettings);
    p->logger                 = std::make_unique<StdOutLogger>();
    p->timeRange              = Range(0._f, 1000._f);
    p->timeStepping           = Factory::getTimestepping(globalSettings, p->storage);

    auto bodySettings = BODY_SETTINGS;
    bodySettings.set(BodySettingsIds::ENERGY, 0.001_f);
    SphericalDomain domain1(Vector(0._f), 1._f);
    Storage body1 = p->model.createParticles(domain1, bodySettings);

    SphericalDomain domain2(Vector(2._f, 1._f, 0._f), 0.3_f);
    Storage body2 = p->model.createParticles(domain2, bodySettings);
    body2.dt<QuantityKey::R>().fill(Vector(-0.4_f, 0._f, 0._f));

    *p->storage = std::move(body1);
    p->storage->merge(body2);


    p->callbacks = std::make_unique<GuiCallbacks>(glPane);
    /*BaseStorage projectile;
    projectile.create(100,
                      new SphericalDomain(Vector(2._f, 1._f, 0._f), 0.3_f),
                      new HexagonalPacking(),
                      BODY_SETTINGS);
    projectile.addVelocity(Vector(-3.f, 0.f, 0.f));
    p1->model.merge(projectile);*/


    worker = std::thread([&p]() { p->run(); });


    return true;
}

MyApp::~MyApp() {
    worker.join();
}
