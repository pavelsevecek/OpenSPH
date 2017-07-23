#include "gui/windows/OrthoPane.h"
#include "gui/Controller.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "system/Profiler.h"
#include "thread/CheckFunction.h"
#include <wx/dcclient.h>
#include <wx/timer.h>

NAMESPACE_SPH_BEGIN

OrthoPane::OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& gui)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , controller(controller) {
    this->SetMinSize(
        wxSize(gui.get<int>(GuiSettingsId::RENDER_WIDTH), gui.get<int>(GuiSettingsId::RENDER_HEIGHT)));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(OrthoPane::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(OrthoPane::onMouseWheel));

    particle.lastIdx = -1;
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // MEASURE_SCOPE("OrthoPane::onPaint");
    wxPaintDC dc(this);
    SharedPtr<Bitmap> bitmap = controller->getRenderedBitmap();
    if (bitmap->isOk()) { // not empty
        dc.DrawBitmap(*bitmap, wxPoint(0, 0));
    }
    /*if (particle.selected) {
        dc.DrawText(std::to_string(particle.selected->getIndex()), wxPoint(10, 10));
    } else {
        dc.DrawText("no particle", wxPoint(10, 10));
    }*/
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    Point position = evt.GetPosition();
    if (evt.Dragging()) {
        Point offset = Point(position.x - dragging.position.x, -(position.y - dragging.position.y));
        SharedPtr<Abstract::Camera> camera = controller->getCurrentCamera();
        camera->pan(offset);
        this->Refresh();
    } else {
        SharedPtr<Abstract::Camera> camera = controller->getCurrentCamera();
        Optional<Particle> selectedParticle = controller->getIntersectedParticle(position);
        const Size selectedIdx = selectedParticle ? selectedParticle->getIndex() : -1;
        if (selectedIdx != particle.lastIdx) {
            particle.lastIdx = selectedIdx;
            controller->setSelectedParticle(selectedParticle);
            this->Refresh();
        }
    }
    dragging.position = position;
    evt.Skip();
}

void OrthoPane::onMouseWheel(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    const float spin = evt.GetWheelRotation();
    const float amount = (spin > 0.f) ? 1.2f : 1.f / 1.2f;
    SharedPtr<Abstract::Camera> camera = controller->getCurrentCamera();
    camera->zoom(amount);
    this->Refresh();
    evt.Skip();
}

NAMESPACE_SPH_END
