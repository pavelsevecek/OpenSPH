#include "gui/gui.h"
#include "gui/callbacks.h"
#include "gui/glpane.h"
#include "wx/glcanvas.h"
#include "wx/sizer.h"
#include "wx/wx.h"

#include "problem/Problem.h"


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


    Problem<BasicModel>* p1 = new Problem<BasicModel>(GLOBAL_SETTINGS);
    //p1->finder              = std::make_unique<KdTree>();
    p1->logger              = std::make_unique<StdOut>();
    //p1->timestepping        = std::make_unique<EulerExplicit>(p1->model, GLOBAL_SETTINGS); /// \todo add model
    p1->timeRange           = Range<Float>(0._f, 10._f);
    //p1->model.create(10000, new SphericalDomain(Vector(0._f), 1._f), new HexagonalPacking(), BODY_SETTINGS);
    p1->callbacks = std::make_unique<GuiCallbacks>(glPane);
    /*BaseStorage projectile;
    projectile.create(100,
                      new SphericalDomain(Vector(2._f, 1._f, 0._f), 0.3_f),
                      new HexagonalPacking(),
                      BODY_SETTINGS);
    projectile.addVelocity(Vector(-3.f, 0.f, 0.f));
    p1->model.merge(projectile);*/


    worker = std::thread([&p1]() { p1->run(); });


    return true;
}

MyApp::~MyApp() {
    worker.join();
}
