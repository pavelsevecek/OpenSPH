#include "gui/windows/PaletteEditor.h"
#include "gui/objects/RenderContext.h"
#include "post/Point.h"
#include <algorithm>

#include <wx/aui/framemanager.h>
#include <wx/colordlg.h>
#include <wx/dcbuffer.h>

NAMESPACE_SPH_BEGIN

const int MARGIN_TOP = 20;
const int MARGIN_LEFT = 20;
const int MARGIN_RIGHT = 20;
const int MARGIN_BOTTOM = 20;

const wxPoint TOP_LEFT = wxPoint(MARGIN_LEFT, MARGIN_TOP);

PaletteEditor::PaletteEditor(wxWindow* parent, const wxSize size, const Palette& palette)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
    , palette(palette) {
    this->SetMinSize(wxSize(320, 100));
    this->SetMaxSize(wxSize(-1, 100));
    this->SetBackgroundStyle(wxBG_STYLE_PAINT);

    this->Connect(wxEVT_PAINT, wxPaintEventHandler(PaletteEditor::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(PaletteEditor::onMouseMotion));
    this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(PaletteEditor::onLeftDown));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(PaletteEditor::onLeftUp));
    this->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(PaletteEditor::onDoubleClick));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(PaletteEditor::onRightUp));

    points = palette.getPoints().clone();
}

void PaletteEditor::setPalette(const Palette& newPalette) {
    palette = newPalette;
    points = palette.getPoints().clone();
    this->Refresh();
}

void PaletteEditor::setPaletteColors(const Palette& newPalette) {
    points = newPalette.getPoints().clone();
    this->updatePaletteFromPoints(false);
}

void PaletteEditor::enable(const bool value) {
    enabled = value;
    this->Refresh();
}

void PaletteEditor::updatePaletteFromPoints(const bool notify) {
    palette = Palette(points.clone(), palette.getInterval(), palette.getScale());
    this->Refresh();

    if (notify) {
        onPaletteChangedByUser.callIfNotNull(palette);
    }
}

void PaletteEditor::onPaint(wxPaintEvent& UNUSED(evt)) {
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    const wxSize size =
        this->GetClientSize() - wxSize(MARGIN_LEFT + MARGIN_RIGHT, MARGIN_TOP + MARGIN_BOTTOM);

    WxRenderContext context(dc);
    Rgba background(dc.GetBackground().GetColour());
    drawPalette(context, Pixel(TOP_LEFT), Pixel(size.x, size.y), palette, background.inverse());

    // bounding rectangle
    dc.SetPen(*wxWHITE_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(TOP_LEFT, size);

    if (!enabled) {
        return;
    }

    // controls
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxWHITE_BRUSH);

    const int thickness = 1;
    const int triSide = 5;
    for (const Palette::Point& p : points) {
        const int x = pointToWindow(p.value);
        dc.DrawRectangle(wxPoint(x - thickness / 2, MARGIN_TOP - 2), wxSize(thickness, size.y + 4));
        wxPoint tri[3];
        tri[0] = wxPoint(x, MARGIN_TOP);
        tri[1] = wxPoint(x - triSide, MARGIN_TOP - triSide);
        tri[2] = wxPoint(x + triSide, MARGIN_TOP - triSide);
        dc.DrawPolygon(3, tri);
        tri[0] = wxPoint(x, MARGIN_TOP + size.y);
        tri[1] = wxPoint(x - triSide, MARGIN_TOP + size.y + triSide);
        tri[2] = wxPoint(x + triSide, MARGIN_TOP + size.y + triSide);
        dc.DrawPolygon(3, tri);
    }
}

Optional<Size> PaletteEditor::lock(const int x) const {
    for (Size i = 0; i < points.size(); ++i) {
        const int p = pointToWindow(points[i].value);
        if (abs(x - p) < 10) {
            return i;
        }
    }
    return NOTHING;
}


float PaletteEditor::windowToPoint(const int x) const {
    const int width = this->GetSize().x - MARGIN_LEFT - MARGIN_RIGHT;
    return float(x - MARGIN_LEFT) / width;
}

int PaletteEditor::pointToWindow(const float x) const {
    const int width = this->GetSize().x - MARGIN_LEFT - MARGIN_RIGHT;
    return x * width + MARGIN_LEFT;
}

void PaletteEditor::onMouseMotion(wxMouseEvent& evt) {
    if (!enabled || !active) {
        return;
    }
    const int x = evt.GetPosition().x;
    const Size index = active.value();
    SPH_ASSERT(std::is_sorted(points.begin(), points.end()));
    points[index].value = clamp(windowToPoint(x), 0.f, 1.f);
    if (index > 0 && points[index].value < points[index - 1].value) {
        std::swap(points[index], points[index - 1]);
        active = index - 1;
    } else if (index < points.size() - 1 && points[index].value > points[index + 1].value) {
        std::swap(points[index], points[index + 1]);
        active = index + 1;
    }
    SPH_ASSERT(std::is_sorted(points.begin(), points.end()));
    this->updatePaletteFromPoints(true);
    this->Refresh();
}

void PaletteEditor::onLeftUp(wxMouseEvent& evt) {
    active = NOTHING;
    evt.Skip(); // required to allow the panel to receive focus
}

void PaletteEditor::onLeftDown(wxMouseEvent& evt) {
    active = this->lock(evt.GetPosition().x);
    evt.Skip(); // required to allow the panel to receive focus
}

void PaletteEditor::onRightUp(wxMouseEvent& evt) {
    if (!enabled) {
        return;
    }
    const Optional<Size> index = this->lock(evt.GetPosition().x);
    if (index && index.value() > 0 && index.value() < points.size() - 1) {
        points.remove(index.value());
        this->updatePaletteFromPoints(true);
        this->Refresh();
    }
}

void PaletteEditor::onDoubleClick(wxMouseEvent& evt) {
    if (!enabled) {
        return;
    }
    Optional<Size> index = this->lock(evt.GetPosition().x);
    if (!index) {
        const float pos = windowToPoint(evt.GetPosition().x);
        for (Size i = 0; i < points.size(); ++i) {
            if (points[i].value > pos) {
                index = i;
                break;
            }
        }
        if (!index) {
            index = points.size();
        }
        Rgba color = palette(palette.relativeToRange(pos));
        points.insert(index.value(), Palette::Point{ pos, color });
    }

    wxColourDialog* dialog = new wxColourDialog(this);
    dialog->GetColourData().SetColour(wxColour(points[index.value()].color));

    if (dialog->ShowModal() == wxID_OK) {
        const wxColourData& data = dialog->GetColourData();
        points[index.value()].color = Rgba(data.GetColour());
    }
    this->updatePaletteFromPoints(true);
    this->Refresh();
}

NAMESPACE_SPH_END
