#include "gui/OrthoPane.h"
#include "gui/Settings.h"
#include "system/Profiler.h"
#include <wx/dcclient.h>

NAMESPACE_SPH_BEGIN

OrthoPane::OrthoPane(wxWindow* parent, const std::shared_ptr<Storage>& storage, const GuiSettings& settings)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , storage(storage)
    , settings(settings) {
    this->SetMinSize(wxSize(640, 480));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(OrthoPane::onMouseMotion));
    this->Connect(wxEVT_MOUSEWHEEL, wxMouseEventHandler(OrthoPane::onMouseWheel));
    this->Connect(wxEVT_TIMER, wxTimerEventHandler(OrthoPane::onTimer));
    setQuantity(QuantityIds::POSITIONS);
    this->fov = 240.f / settings.get<Float>(GuiSettingsIds::VIEW_FOV);
    refreshTimer = new wxTimer(this, 1); /// \todo check that timer is destroyed
    refreshTimer->Start(50);
}

OrthoPane::~OrthoPane() = default;


wxPoint OrthoPane::project(const OrthoEnum projection, const wxSize size, const Vector& r) {
    switch (projection) {
    case OrthoEnum::XY:
        return wxPoint(center.x + r[X] * fov, size.y - (center.y + r[Y] * fov) - 1);
    case OrthoEnum::XZ:
        return wxPoint(center.x + r[X] * fov, size.y - (center.y + r[Z] * fov) - 1);
    case OrthoEnum::YZ:
        return wxPoint(center.x + r[Y] * fov, size.y - (center.y + r[Z] * fov) - 1);
    default:
        NOT_IMPLEMENTED;
    }
}

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    MEASURE_SCOPE("OrthoPane::onPaint");
    // called from main thread
    wxPaintDC dc(this);
    wxBitmap bitmap(dc.GetSize());
    wxMemoryDC memoryDc(bitmap);
    memoryDc.SetBrush(*wxBLACK_BRUSH);
    memoryDc.DrawRectangle(wxPoint(0, 0), dc.GetSize());
    const float radius = settings.get<Float>(GuiSettingsIds::PARTICLE_RADIUS);
    const OrthoEnum projection = settings.get<OrthoEnum>(GuiSettingsIds::ORTHO_PROJECTION);
    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);
    for (Size i = 0; i < displayedIdxs.second().size(); ++i) {
        const Size idx = displayedIdxs.second()[i];
        brush.SetColour(colors.second()[idx]);
        pen.SetColour(colors.second()[idx]);
        memoryDc.SetBrush(brush);
        memoryDc.SetPen(pen);
        const Vector& r = positions[idx];
        memoryDc.DrawCircle(project(projection, dc.GetSize(), r), max(float(r[H]) * fov * radius, 1.f));
    }
    dc.DrawBitmap(bitmap, wxPoint(0, 0));

    drawPalette(dc);
}

void OrthoPane::drawPalette(wxPaintDC& dc) {
    const int size = 201;
    wxPoint origin(dc.GetSize().x - 50, size + 30);
    wxPen pen = dc.GetPen();
    for (int i = 0; i < size; ++i) {
        const float value = palette.getInterpolatedValue(float(i) / (size - 1));
        wxColour color = palette(value);
        pen.SetColour(color);
        dc.SetPen(pen);
        dc.DrawLine(wxPoint(origin.x, origin.y - i), wxPoint(origin.x + 30, origin.y - i));
        if (i % 50 == 0) {
            dc.SetTextForeground(Color::white());
            std::stringstream ss;
            ss << std::setprecision(1) << std::scientific << value;
            const std::string text = ss.str();
            wxSize extent = dc.GetTextExtent(text);
            dc.DrawText(text, wxPoint(origin.x - 50, origin.y - i - (extent.y >> 1)));
        }
    }
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    wxPoint position = evt.GetPosition();
    if (evt.Dragging()) {
        center.x += position.x - lastMousePosition.x;
        center.y -= position.y - lastMousePosition.y;
        this->Refresh();
    }
    lastMousePosition = position;
    evt.Skip();
}

void OrthoPane::onMouseWheel(wxMouseEvent& evt) {
    const float spin = evt.GetWheelRotation() / 120.f;
    if (spin > 0.f) {
        fov *= 1.2f;
    } else {
        fov *= 1.f / 1.2f;
    }
    this->Refresh();
    evt.Skip();
}

void OrthoPane::onTimer(wxTimerEvent& evt) {
    this->Refresh();
    evt.Skip();
}

void OrthoPane::draw(const std::shared_ptr<Storage>& newStorage) {
    MEASURE_SCOPE("OrthoPane::draw");
    // called from worker thread, cannot touch wx stuff
    storage = newStorage;
    displayedIdxs->clear();
    const Float cutoff = settings.get<Float>(GuiSettingsIds::ORTHO_CUTOFF);
    for (Size i = 0; i < positions.size(); ++i) {
        if (abs(positions[i][Z]) < cutoff) {
            displayedIdxs->push(i);
        }
    }
    update();
    displayedIdxs.swap();
    colors.swap();
}

void OrthoPane::update() {
    MEASURE_SCOPE("OrthoPane::update");
    ASSERT(storage);
    /// \todo copy, avoid allocation
    positions = storage->getValue<Vector>(QuantityIds::POSITIONS).clone();
    colors->clear();
    switch (quantity) {
    case QuantityIds::POSITIONS: {
        ArrayView<Vector> v = storage->getAll<Vector>(QuantityIds::POSITIONS)[1];
        for (Size i = 0; i < v.size(); ++i) {
            colors->push(palette(getLength(v[i])));
        }
        break;
    }
    case QuantityIds::DEVIATORIC_STRESS: {
        ArrayView<TracelessTensor> s = storage->getValue<TracelessTensor>(QuantityIds::DEVIATORIC_STRESS);
        for (Size i = 0; i < s.size(); ++i) {
            colors->push(palette(sqrt(ddot(s[i], s[i]))));
        }
        break;
    }
    default: {
        ArrayView<Float> values = storage->getValue<Float>(quantity);
        for (Size i = 0; i < values.size(); ++i) {
            colors->push(palette(values[i]));
        }
        break;
    }
    }
}

void OrthoPane::setQuantity(const QuantityIds key) {
    quantity = key;
    Range range;
    switch (key) {
    case QuantityIds::POSITIONS:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_VELOCITY);
        break;
    case QuantityIds::DENSITY:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_DENSITY);
        break;
    case QuantityIds::PRESSURE:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_PRESSURE);
        break;
    case QuantityIds::ENERGY:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_ENERGY);
        break;
    case QuantityIds::DEVIATORIC_STRESS:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_STRESS);
        break;
    case QuantityIds::DAMAGE:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_DAMAGE);
        break;
    default:
        NOT_IMPLEMENTED;
    }
    palette = Palette::forQuantity(key, range);
    update();
    this->Refresh();
}


NAMESPACE_SPH_END
