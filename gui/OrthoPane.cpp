#include "gui/OrthoPane.h"
#include "gui/Settings.h"
#include <wx/dcclient.h>

NAMESPACE_SPH_BEGIN

OrthoPane::OrthoPane(wxWindow* parent, const GuiSettings& settings)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
    , settings(settings) {
    this->SetMinSize(wxSize(640, 480));
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(OrthoPane::OnPaint));
}

OrthoPane::~OrthoPane() = default;

void OrthoPane::OnPaint(wxPaintEvent& UNUSED(evt)) {
    wxPaintDC dc(this);
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.DrawRectangle(wxPoint(0, 0), dc.GetSize());
    wxPoint center(320, 240);
    const int fov = int(240.f / settings.get<Float>(GuiSettingsIds::VIEW_FOV));
    for (int i : displayedIdxs) {
        dc.SetBrush(*wxBLACK_BRUSH);
        const Vector& r = positions[i];
        dc.DrawCircle(wxPoint(center.x + r[X] * fov, center.y + r[Y] * fov), r[H] * fov);
    }
}

void OrthoPane::draw(const std::shared_ptr<Storage>& newStorage) {
    storage   = newStorage;
    positions = storage->getValue<Vector>(QuantityKey::POSITIONS);
    displayedIdxs.clear();
    const Float cutoff = settings.get<Float>(GuiSettingsIds::ORTHO_CUTOFF);
    for (int i = 0; i < positions.size(); ++i) {
        if (Math::abs(positions[i][Z]) < cutoff) {
            displayedIdxs.push(i);
        }
    }
    this->Refresh();
}


NAMESPACE_SPH_END
