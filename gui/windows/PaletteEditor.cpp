#include "gui/windows/PaletteEditor.h"
#include "gui/Utils.h"
#include "gui/windows/PaletteDialog.h"
#include "post/Plot.h"
#include "post/Point.h"
#include <algorithm>
#include <wx/aui/framemanager.h>

NAMESPACE_SPH_BEGIN

const int MARGIN_TOP = 20;
const int MARGIN_LEFT = 20;
const int MARGIN_RIGHT = 20;
const int MARGIN_BOTTOM = 20;

const wxPoint TOP_LEFT = wxPoint(MARGIN_LEFT, MARGIN_TOP);

PaletteEditor::PaletteEditor(wxWindow* parent, const wxSize size, const Palette& palette)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, size) {
    this->SetMinSize(wxSize(320, 100));
    this->SetMaxSize(wxSize(-1, 100));
    this->SetBackgroundStyle(wxBG_STYLE_PAINT);

    this->Connect(wxEVT_PAINT, wxPaintEventHandler(PaletteEditor::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(PaletteEditor::onMouseMotion));
    this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(PaletteEditor::onLeftDown));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(PaletteEditor::onLeftUp));
    this->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(PaletteEditor::onDoubleClick));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(PaletteEditor::onRightUp));

    for (const Palette::Point& p : palette.getPoints()) {
        points.push(p);
    }
}

void PaletteEditor::onPaint(wxPaintEvent& UNUSED(evt)) {
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    const wxSize size =
        this->GetClientSize() - wxSize(MARGIN_LEFT + MARGIN_RIGHT, MARGIN_TOP + MARGIN_BOTTOM);

    drawPalette(dc, TOP_LEFT, size, Palette(points.clone()));

    // bounding rectangle
    dc.SetPen(*wxWHITE_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(TOP_LEFT, size);

    // tics
    /*Array<Float> tics;
    if (palette.getScale() == PaletteScale::LOGARITHMIC) {
        tics = getLogTics(palette.getInterval(), 4);
    } else {
        tics = getLinearTics(palette.getInterval(), 4);
    }
    for (Float tic : tics) {
        const float value = palette.paletteToRelative(tic);
        const int i = int(value * size.x);
        dc.DrawLine(wxPoint(i, 0) + TOP_LEFT, wxPoint(i, 6) + TOP_LEFT);
        dc.DrawLine(wxPoint(i, size.y) + TOP_LEFT, wxPoint(i, size.y - 6) + TOP_LEFT);

        String text = toPrintableString(tic, 1, 1000);
        dc.DrawText(text.toUnicode(), wxPoint(i, -15) + TOP_LEFT);
    }*/

    // controls
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxWHITE_BRUSH);

    const int thickness = 5;
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

float PaletteEditor::windowToPoint(const int x) const {
    const int width = this->GetSize().x - MARGIN_LEFT - MARGIN_RIGHT;
    return float(x - MARGIN_LEFT) / width;
}

int PaletteEditor::pointToWindow(const float x) const {
    const int width = this->GetSize().x - MARGIN_LEFT - MARGIN_RIGHT;
    return x * width + MARGIN_LEFT;
}

void PaletteEditor::onMouseMotion(wxMouseEvent& evt) {
    if (!active) {
        return;
    }
    const int x = evt.GetPosition().x;
    const Size index = active.value();
    points[index].value = clamp(windowToPoint(x), 0.f, 1.f);
    if (index > 0 && points[index].value < points[index - 1].value) {
        std::swap(points[index], points[index - 1]);
        active = index - 1;
    } else if (index < points.size() - 1 && points[index].value > points[index + 1].value) {
        std::swap(points[index], points[index + 1]);
        active = index + 1;
    }
    onPaletteChanged.callIfNotNull(this->getPalette());

    this->Refresh();
}


void PaletteEditor::onDoubleClick(wxMouseEvent& evt) {
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
        const Palette palette = this->getPalette();
        points.insert(index.value(), Palette::Point{ pos, palette(pos) });
    }

    wxColourDialog* dialog = new wxColourDialog(this);
    dialog->GetColourData().SetColour(wxColour(points[index.value()].color));

    if (dialog->ShowModal() == wxID_OK) {
        const wxColourData& data = dialog->GetColourData();
        points[index.value()].color = Rgba(data.GetColour());
    }
    onPaletteChanged.callIfNotNull(this->getPalette());
    this->Refresh();
}

String PaletteEntry::toString() const {
    StringTextOutputStream ss;
    palette.saveToStream(ss);
    String s = ss.toString();
    s.replaceAll("\n", ";");
    return s;
}

void PaletteEntry::fromString(const String& s) {
    String expanded = s;
    expanded.replaceAll(";", "\n");
    StringTextInputStream ss(expanded);
    palette.loadFromStream(ss);
}

PalettePreview::PalettePreview(wxWindow* parent,
    const wxPoint point,
    const wxSize size,
    const Palette& palette)
    : wxPanel(parent, wxID_ANY, point, size)
    , palette(palette) {
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(PalettePreview::onPaint));
}

void PalettePreview::onPaint(wxPaintEvent& UNUSED(evt)) {
    wxPaintDC dc(this);
    wxSize size = this->GetSize();
    drawPalette(dc, wxPoint(0, 0), size, palette);
}


wxPGWindowList PalettePgEditor::CreateControls(wxPropertyGrid* propgrid,
    wxPGProperty* property,
    const wxPoint& pos,
    const wxSize& size) const {
    PaletteProperty* paletteProp = dynamic_cast<PaletteProperty*>(property);
    SPH_ASSERT(paletteProp);

    PaletteEditor* panel = nullptr;
    PalettePreview* preview = new PalettePreview(propgrid, pos, size, paletteProp->getPalette());

    wxAuiPaneInfo info = aui->GetPane("PaletteEditor");
    if (!info.IsOk()) {
        panel = new PaletteEditor(propgrid->GetParent(), wxSize(300, 200), paletteProp->getPalette());

        info = wxAuiPaneInfo();
        info.Name("PaletteEditor")
            .Left()
            .MinSize(wxSize(300, -1))
            .Position(1)
            .CaptionVisible(true)
            .DockFixed(false)
            .CloseButton(true)
            .DestroyOnClose(true)
            .Caption("Palette");
        aui->AddPane(panel, info);
        aui->Update();
    } else {
        panel = dynamic_cast<PaletteEditor*>(info.window);
        SPH_ASSERT(panel);
        panel->setPalette(paletteProp->getPalette());
        panel->Refresh();
    }


    panel->setPaletteChangedCallback(
        [paletteProp, preview = wxWeakRef<PalettePreview>(preview)](const Palette& palette) {
            paletteProp->setPalete(palette);
            if (preview) {
                preview->setPalette(palette);
            }
        });

    return wxPGWindowList(preview);
}

void PalettePgEditor::UpdateControl(wxPGProperty* property, wxWindow* ctrl) const {
    (void)property;
    (void)ctrl;
}

void PalettePgEditor::DrawValue(wxDC& dc,
    const wxRect& rect,
    wxPGProperty* property,
    const wxString& UNUSED(text)) const {
    PaletteProperty* paletteProp = dynamic_cast<PaletteProperty*>(property);
    SPH_ASSERT(paletteProp);

    drawPalette(dc, rect.GetPosition(), rect.GetSize(), paletteProp->getPalette());


    (void)rect;
    (void)property;
}

bool PalettePgEditor::OnEvent(wxPropertyGrid* propgrid,
    wxPGProperty* property,
    wxWindow* wnd_primary,
    wxEvent& event) const {
    (void)propgrid;
    (void)property;
    (void)wnd_primary;
    (void)event;

    return false;
}


NAMESPACE_SPH_END
