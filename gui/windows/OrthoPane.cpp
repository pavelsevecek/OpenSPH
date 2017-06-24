#include "gui/windows/OrthoPane.h"
#include "gui/Controller.h"
#include "gui/Factory.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/objects/Bitmap.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Element.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <wx/dcclient.h>
#include <wx/timer.h>

NAMESPACE_SPH_BEGIN

void OrthoRenderer::initialize(ArrayView<const Vector> r,
    const Abstract::Element& element,
    const Abstract::Camera& camera) {
    cached.idxs.clear();
    cached.positions.clear();
    cached.colors.clear();

    for (Size i = 0; i < r.size(); ++i) {
        const Color color = element.eval(i);
        const Optional<ProjectedPoint> p = camera.project(r[i]);
        if (p) {
            cached.idxs.push(i);
            cached.positions.push(r[i]);
            cached.colors.push(color);
        }
    }

    cached.palette = element.getPalette();
}

SharedPtr<Bitmap> OrthoRenderer::render(const Abstract::Camera& camera,
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

    // draw particles
    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);
    for (Size i = 0; i < cached.positions.size(); ++i) {
        if (cached.idxs[i] == params.selectedParticle) {
            brush.SetColour(*wxRED);
            pen.SetColour(*wxWHITE);
        } else {
            brush.SetColour(cached.colors[i]);
            pen.SetColour(cached.colors[i]);
        }
        dc.SetBrush(brush);
        dc.SetPen(pen);
        const Optional<ProjectedPoint> p = camera.project(cached.positions[i]);
        ASSERT(p); // cached values must be visible by the camera
        const int size = max(int(p->radius * params.particles.scale), 1);
        dc.DrawCircle(p->point, size);
    }
    if (cached.palette) {
        this->drawPalette(dc, cached.palette.value());
    }
    const Float time = stats.get<Float>(StatisticsId::TOTAL_TIME);
    dc.DrawText(("t = " + std::to_string(time) + "s").c_str(), wxPoint(0, 0));

    dc.SelectObject(wxNullBitmap);
    return makeShared<Bitmap>(std::move(bitmap));
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
            dc.SetPen(*wxWHITE_PEN);
            dc.DrawLine(wxPoint(origin.x, origin.y - i), wxPoint(origin.x + 6, origin.y - i));
            dc.DrawLine(wxPoint(origin.x + 24, origin.y - i), wxPoint(origin.x + 30, origin.y - i));
            dc.SetTextForeground(Color::white());
            wxFont font(10, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            dc.SetFont(font);
            std::wstring text = toPrintableString(value, 1, 1000);
            wxSize extent = dc.GetTextExtent(text);
            drawTextWithSubscripts(dc, text, wxPoint(origin.x - 80, origin.y - i - (extent.y >> 1)));
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

    particle.lastIdx = -1;
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    MEASURE_SCOPE("OrthoPane::onPaint");
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
