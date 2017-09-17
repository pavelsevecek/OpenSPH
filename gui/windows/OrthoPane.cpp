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
    const int width = gui.get<int>(GuiSettingsId::RENDER_WIDTH);
    const int height = gui.get<int>(GuiSettingsId::RENDER_HEIGHT);
    this->SetMinSize(wxSize(width, height));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(OrthoPane::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(OrthoPane::onMouseWheel));
    this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(OrthoPane::onRightDown));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(OrthoPane::onRightUp));

    particle.lastIdx = -1;
    arcBall.resize(Point(width, height));
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
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    Point position = evt.GetPosition();
    if (evt.Dragging()) {
        SharedPtr<ICamera> camera = controller->getCurrentCamera();
        Point offset = Point(position.x - dragging.position.x, -(position.y - dragging.position.y));
        if (evt.RightIsDown()) {
            // right button, rotate view
            AffineMatrix matrix = arcBall.drag(position);
            camera->transform(dragging.initialMatrix * matrix);

            // needs to re-initialize the renderer
            // controller->tryRedraw();
        } else {
            // left button (or middle), pan
            camera->pan(offset);
        }
        this->Refresh();
    } else {
        Optional<Particle> selectedParticle = controller->getIntersectedParticle(position);
        const Size selectedIdx = selectedParticle ? selectedParticle->getIndex() : -1;
        if (selectedIdx != particle.lastIdx) {
            particle.lastIdx = selectedIdx;
            controller->setSelectedParticle(selectedParticle);
            this->Refresh();
        }
    }
    dragging.position = position;
}

void OrthoPane::onRightDown(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    arcBall.click(evt.GetPosition());
}

void OrthoPane::onRightUp(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    AffineMatrix matrix = arcBall.drag(evt.GetPosition());
    // camera->transform(dragging.initialMatrix * matrix);
    dragging.initialMatrix = dragging.initialMatrix * matrix;
}

void OrthoPane::onMouseWheel(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    const float spin = evt.GetWheelRotation();
    const float amount = (spin > 0.f) ? 1.2f : 1.f / 1.2f;
    SharedPtr<ICamera> camera = controller->getCurrentCamera();
    camera->zoom(amount);
    this->Refresh();
}

NAMESPACE_SPH_END
