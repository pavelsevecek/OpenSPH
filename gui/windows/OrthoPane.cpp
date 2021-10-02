#include "gui/windows/OrthoPane.h"
#include "gui/Controller.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/windows/PaletteDialog.h"
#include "system/Profiler.h"
#include "thread/CheckFunction.h"
#include <wx/dcbuffer.h>
#include <wx/timer.h>

NAMESPACE_SPH_BEGIN

OrthoPane::OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& UNUSED(gui))
    : IGraphicsPane(parent)
    , controller(controller) {
    this->SetBackgroundStyle(wxBG_STYLE_PAINT);

    this->SetMinSize(wxSize(300, 300));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(OrthoPane::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(OrthoPane::onMouseWheel));
    this->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(OrthoPane::onRightDown));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(OrthoPane::onRightUp));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(OrthoPane::onLeftUp));
    this->Connect(wxEVT_SIZE, wxSizeEventHandler(OrthoPane::onResize));

    // get the camera from controller; note that since then, we provided the updated camera to controller from
    // here, the camera is never modified in controller!
    camera = controller->getCurrentCamera();
    particle.lastIdx = -1;

    const wxSize size = this->GetSize();
    arcBall.resize(Pixel(size.x, size.y));
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::resetView() {
    dragging.initialMatrix = AffineMatrix::identity();
    camera->transform(AffineMatrix::identity());
}

void OrthoPane::onTimeStep(const Storage& storage, const Statistics& UNUSED(stats)) {
    if (controller->getParams().get<bool>(GuiSettingsId::CAMERA_AUTOSETUP)) {
        camera->autoSetup(storage);
    }
}

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);

    wxAutoBufferedPaintDC dc(this);
    const wxBitmap& bitmap = controller->getRenderedBitmap();
    if (!bitmap.IsOk()) {
        dc.Clear();
        return;
    }

    dc.DrawBitmap(bitmap, wxPoint(0, 0));
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    Pixel position(evt.GetPosition());
    if (evt.Dragging()) {
        Pixel offset = Pixel(position.x - dragging.position.x, -(position.y - dragging.position.y));
        if (evt.RightIsDown()) {
            // right button, rotate view
            AffineMatrix matrix = arcBall.drag(position, Vector(0._f));
            camera->transform(dragging.initialMatrix * matrix);
        } else {
            // left button (or middle), pan
            camera->pan(offset);
        }
        controller->refresh(camera->clone());
    }
    dragging.position = position;
}

void OrthoPane::onRightDown(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    arcBall.click(Pixel(evt.GetPosition()));
}

void OrthoPane::onRightUp(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    AffineMatrix matrix = arcBall.drag(Pixel(evt.GetPosition()), Vector(0._f));
    dragging.initialMatrix = dragging.initialMatrix * matrix;
}

void OrthoPane::onLeftUp(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    Pixel position(evt.GetPosition());
    Optional<Size> selectedIdx = controller->getIntersectedParticle(position, 1.f);
    if (selectedIdx.valueOr(-1) != particle.lastIdx.valueOr(-1)) {
        particle.lastIdx = selectedIdx;
        controller->setSelectedParticle(selectedIdx);
        controller->refresh(camera->clone());
    }
}

void OrthoPane::onMouseWheel(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    const float spin = evt.GetWheelRotation();
    const float amount = (spin > 0.f) ? 1.2f : 1.f / 1.2f;
    Pixel fixedPoint(evt.GetPosition());
    camera->zoom(fixedPoint, amount);
    controller->refresh(camera->clone());
    controller->setAutoZoom(false);
}

void OrthoPane::onResize(wxSizeEvent& evt) {
    const Pixel newSize(max(10, evt.GetSize().x), max(10, evt.GetSize().y));
    arcBall.resize(newSize);
    camera->resize(newSize);
    controller->tryRedraw();
}

NAMESPACE_SPH_END
