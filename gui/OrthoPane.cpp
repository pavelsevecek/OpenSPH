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
    setQuantity(QuantityKey::POSITIONS);
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::onPaint(wxPaintEvent& UNUSED(evt)) {
    MEASURE_SCOPE("OrthoPane::onPaint");
    wxPaintDC dc(this);
    wxBitmap bitmap(dc.GetSize());
    wxMemoryDC memoryDc(bitmap);
    memoryDc.SetBrush(*wxWHITE_BRUSH);
    memoryDc.DrawRectangle(wxPoint(0, 0), dc.GetSize());
    const int fov      = int(240.f / settings.get<Float>(GuiSettingsIds::VIEW_FOV));
    const float radius = settings.get<Float>(GuiSettingsIds::PARTICLE_RADIUS);
    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);
    for (int i : displayedIdxs) {
        brush.SetColour(colors[i]);
        pen.SetColour(colors[i]);
        memoryDc.SetBrush(brush);
        memoryDc.SetPen(pen);
        const Vector& r = positions[i];
        memoryDc.DrawCircle(wxPoint(center.x + r[X] * fov, center.y + r[Y] * fov), r[H] * fov * radius);
    }
    dc.DrawBitmap(bitmap, wxPoint(0, 0));
}

void OrthoPane::onMouseMotion(wxMouseEvent& evt) {
    wxPoint position = evt.GetPosition();
    if (evt.Dragging()) {
        center += position - lastMousePosition;
        this->Refresh();
    }
    lastMousePosition = position;
    evt.Skip();
}

void OrthoPane::draw(const std::shared_ptr<Storage>& newStorage) {
    MEASURE_SCOPE("OrthoPane::draw");
    storage = newStorage;
    update();
    displayedIdxs.clear();
    const Float cutoff = settings.get<Float>(GuiSettingsIds::ORTHO_CUTOFF);
    for (Size i = 0; i < positions.size(); ++i) {
        if (Math::abs(positions[i][Z]) < cutoff) {
            displayedIdxs.push(i);
        }
    }
    this->Refresh();
}

void OrthoPane::update() {
    MEASURE_SCOPE("OrthoPane::update");
    ASSERT(storage);
    /// \todo copy, avoid allocation
    positions = storage->getValue<Vector>(QuantityKey::POSITIONS).clone();
    colors.clear();
    switch (quantity) {
    case QuantityKey::POSITIONS: {
        ArrayView<Vector> v = storage->getAll<Vector>(QuantityKey::POSITIONS)[1];
        for (Size i = 0; i < v.size(); ++i) {
            colors.push(palette(getLength(v[i])));
        }
        break;
    }
    case QuantityKey::DENSITY: {
        ArrayView<Float> rho = storage->getValue<Float>(QuantityKey::DENSITY);
        for (Size i = 0; i < rho.size(); ++i) {
            colors.push(palette(rho[i]));
        }
        break;
    }
    case QuantityKey::PRESSURE: {
        ArrayView<Float> p = storage->getValue<Float>(QuantityKey::PRESSURE);
        for (Size i = 0; i < p.size(); ++i) {
            colors.push(palette(p[i]));
        }
        break;
    }
    case QuantityKey::DAMAGE: {
        ArrayView<Float> d = storage->getValue<Float>(QuantityKey::DAMAGE);
        for (Size i = 0; i < d.size(); ++i) {
            colors.push(palette(d[i]));
        }
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
}

void OrthoPane::setQuantity(const QuantityKey key) {
    quantity = key;
    Range range;
    switch (key) {
    case QuantityKey::POSITIONS:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_VELOCITY);
        break;
    case QuantityKey::DENSITY:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_DENSITY);
        break;
    case QuantityKey::PRESSURE:
        range = settings.get<Range>(GuiSettingsIds::PALETTE_PRESSURE);
        break;
    case QuantityKey::DAMAGE:
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
