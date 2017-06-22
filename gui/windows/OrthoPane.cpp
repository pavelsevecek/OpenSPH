#include "gui/windows/OrthoPane.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/Settings.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Element.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <wx/dcclient.h>
#include <wx/timer.h>

NAMESPACE_SPH_BEGIN

Bitmap OrthoRenderer::render(ArrayView<const Vector> r,
    const Abstract::Element& element,
    const Abstract::Camera& camera,
    const RenderParams& params,
    Statistics& stats) const {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    MEASURE_SCOPE("OrthoRenderer::render");
    const wxSize size(params.size.x, params.size.y);
    wxBitmap bitmap(size, 24);
    wxMemoryDC dc(bitmap);

    // draw black background (there is no fill method?)
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(wxPoint(0, 0), size);
    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);
    for (Size i = 0; i < r.size(); ++i) {
        const Color color = element.eval(i);
        brush.SetColour(color);
        pen.SetColour(color);
        dc.SetBrush(brush);
        dc.SetPen(pen);
        const Optional<ProjectedPoint> p = camera.project(r[i]);
        if (p) {
            const int size = max(int(p->radius * params.particles.scale), 1);
            dc.DrawCircle(p->point, size);
        }
    }
    Optional<Palette> palette = element.getPalette();
    if (palette) {
        this->drawPalette(dc, palette.value());
    }
    const Float time = stats.get<Float>(StatisticsId::TOTAL_TIME);
    dc.DrawText(("t = " + std::to_string(time) + "s").c_str(), wxPoint(0, 0));

    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

void OrthoRenderer::drawPalette(wxDC& dc, const Palette& palette) const {
    const int size = 201;
    wxPoint origin(dc.GetSize().x - 50, size + 30);
    wxPen pen = dc.GetPen();
    for (Size i = 0; i < size; ++i) {
        const float value = palette.getInterpolatedValue(float(i) / (size - 1));
        wxColour color = palette(value);
        pen.SetColour(color);
        dc.SetPen(pen);
        dc.DrawLine(wxPoint(origin.x, origin.y - i), wxPoint(origin.x + 30, origin.y - i));
        if (i % 50 == 0) {
            dc.SetTextForeground(Color::white());
            wxFont font(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            dc.SetFont(font);
            std::stringstream ss;
            ss << std::setprecision(1) << std::scientific << value;
            const wxString text = ss.str();
            wxSize extent = dc.GetTextExtent(text);
            dc.DrawText(text, wxPoint(origin.x - 60, origin.y - i - (extent.y >> 1)));
        }
    }
}

OrthoPane::OrthoPane(wxWindow* parent, Controller* controller, const GuiSettings& gui)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , controller(controller) {
    this->SetMinSize(
        wxSize(gui.get<int>(GuiSettingsId::RENDER_WIDTH), gui.get<int>(GuiSettingsId::RENDER_HEIGHT)));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(OrthoPane::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(OrthoPane::onMouseWheel));

    camera = Factory::getCamera(gui);
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    MEASURE_SCOPE("OrthoPane::onPaint");
    wxPaintDC dc(this);
    Bitmap bitmap = controller->getRenderedBitmap(*camera);
    if (bitmap.isOk()) { // not empty
        dc.DrawBitmap(bitmap, wxPoint(0, 0));
    }
    if (selected.particle) {
    }
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    Point position = evt.GetPosition();
    if (evt.Dragging()) {
        Point offset = Point(position.x - dragging.position.x, -(position.y - dragging.position.y));
        ASSERT(camera);
        camera->pan(offset);
        this->Refresh();
    } else {
        ASSERT(camera);
        selected.particle = controller->getIntersectedParticle(*camera, position);
    }
    dragging.position = position;
    evt.Skip();
}

void OrthoPane::onMouseWheel(wxMouseEvent& evt) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    const float spin = evt.GetWheelRotation();
    const float amount = (spin > 0.f) ? 1.2f : 1.f / 1.2f;
    ASSERT(camera);
    camera->zoom(amount);
    this->Refresh();
    evt.Skip();
}

NAMESPACE_SPH_END
