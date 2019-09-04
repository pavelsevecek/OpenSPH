#include "gui/windows/CurveDialog.h"
#include "gui/Utils.h"
#include "post/Plot.h"
#include "post/Point.h"
#include <wx/graphics.h>

NAMESPACE_SPH_BEGIN

CurvePanel::CurvePanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(800, 600)) {
    this->Connect(wxEVT_PAINT, wxPaintEventHandler(CurvePanel::onPaint));
    this->Connect(wxEVT_MOTION, wxMouseEventHandler(CurvePanel::onMouseMotion));
    this->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(CurvePanel::onLeftDown));
    this->Connect(wxEVT_LEFT_UP, wxMouseEventHandler(CurvePanel::onLeftUp));
    this->Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(CurvePanel::onRightUp));
}

const float radius = 6;

void CurvePanel::onPaint(wxPaintEvent& UNUSED(evt)) {
    wxPaintDC dc(this);

    dc.SetPen(*wxWHITE_PEN);

    wxGraphicsContext* gc = wxGraphicsContext::Create(dc);
    wxPen pen = *wxWHITE_PEN;
    pen.SetWidth(2);
    gc->SetPen(pen);
    wxBrush brush = *wxWHITE_BRUSH;
    brush.SetColour(wxColour(100, 100, 100));
    gc->SetBrush(brush);

    // draw the curve
    for (Size i = 0; i < curve.getPointCnt() - 1; ++i) {
        wxGraphicsPath path = gc->CreatePath();
        const int x1 = curveToWindow<wxPoint>(curve.getPoint(i)).x;
        const int x2 = curveToWindow<wxPoint>(curve.getPoint(i + 1)).x;

        if (i == highlightSegment) {
            pen.SetColour(wxColour(255, 100, 50));
        } else {
            pen.SetColour(wxColour(180, 180, 180));
        }
        gc->SetPen(pen);

        for (int x = x1; x <= x2; ++x) {
            const float f = curve(windowToCurve(wxPoint(x, 0)).x);
            const float y = curveToWindow<wxPoint2DDouble>(CurvePoint{ 0.f, f }).m_y;
            const wxPoint2DDouble p(x, y);
            if (x > padding) {
                path.AddLineToPoint(p);
            } else {
                path.MoveToPoint(p);
            }
        }
        gc->StrokePath(path);
    }

    // draw the points
    pen.SetColour(wxColour(180, 180, 180));
    gc->SetPen(pen);
    for (Size i = 0; i < curve.getPointCnt(); ++i) {
        const wxPoint p = curveToWindow<wxPoint>(curve.getPoint(i));
        if (highlightIdx == i) {
            brush.SetColour(wxColour(255, 100, 50));
        } else {
            brush.SetColour(wxColour(100, 100, 100));
        }
        gc->SetBrush(brush);
        gc->DrawEllipse(p.x - radius, p.y - radius, 2 * radius, 2 * radius);
    }

    // draw axes
    wxSize size = this->GetSize() - wxSize(2 * padding, 2 * padding);
    dc.DrawLine(wxPoint(padding, padding + size.y), wxPoint(padding + size.x, padding + size.y));
    dc.DrawLine(wxPoint(padding, padding), wxPoint(padding, padding + size.y));

    // draw tics
    const Interval rangeX = curve.rangeX();
    Array<Float> ticsX = getLinearTics(rangeX, 4);
    for (Size i = 0; i < ticsX.size(); ++i) {
        const std::wstring label = toPrintableString(ticsX[i], 1);
        const int x = padding + i * size.x / (ticsX.size() - 1);
        drawTextWithSubscripts(dc, label, wxPoint(x - 6, size.y + padding + 6));
        dc.DrawLine(wxPoint(x, size.y + padding - 2), wxPoint(x, size.y + padding + 2));
    }
    const Interval rangeY = curve.rangeY();
    Array<Float> ticsY = getLinearTics(rangeY, 3);
    for (Size i = 0; i < ticsY.size(); ++i) {
        const std::wstring label = toPrintableString(ticsY[i], 1);
        const int y = padding + size.y - i * size.y / (ticsY.size() - 1);
        drawTextWithSubscripts(dc, label, wxPoint(2, y - 8));
        dc.DrawLine(wxPoint(padding - 2, y), wxPoint(padding + 2, y));
    }

    // draw mouse position and curve value
    if (mousePosition != wxDefaultPosition) {
        dc.SetPen(*wxGREY_PEN);
        // project the mouse position to the curve
        CurvePoint curvePos = windowToCurve(mousePosition);
        curvePos.y = curve(curvePos.x);
        const wxPoint center = curveToWindow<wxPoint>(curvePos);
        dc.DrawLine(wxPoint(padding, center.y), wxPoint(padding + size.x, center.y));
        dc.DrawLine(wxPoint(center.x, padding), wxPoint(center.x, padding + size.y));
        dc.SetTextForeground(wxColour(128, 128, 128));
        wxFont font = dc.GetFont().Smaller();
        dc.SetFont(font);
        const std::wstring labelX = toPrintableString(curvePos.x, 2);
        const std::wstring labelY = toPrintableString(curvePos.y, 2);
        drawTextWithSubscripts(dc, L"(" + labelX, center + wxPoint(-65, -15));
        drawTextWithSubscripts(dc, labelY + L")", center + wxPoint(5, -15));
    }

    delete gc;
}

void CurvePanel::onMouseMotion(wxMouseEvent& evt) {
    mousePosition = evt.GetPosition();
    highlightIdx = this->getIdx(mousePosition);
    highlightSegment = this->getSegment(mousePosition);
    if (evt.Dragging() && lockedIdx) {
        CurvePoint newPos = windowToCurve(evt.GetPosition());
        curve.setPoint(lockedIdx.value(), newPos);
    }
    this->Refresh();
}

void CurvePanel::onLeftDown(wxMouseEvent& evt) {
    mousePosition = evt.GetPosition();
    Optional<Size> idx = this->getIdx(mousePosition);
    if (idx) {
        lockedIdx = idx;
    } else {
        const CurvePoint newPos = windowToCurve(mousePosition);
        curve.addPoint(newPos);
        lockedIdx = this->getIdx(mousePosition);
    }
    this->Refresh();
}

void CurvePanel::onLeftUp(wxMouseEvent& UNUSED(evt)) {
    lockedIdx = NOTHING;
}

void CurvePanel::onRightUp(wxMouseEvent& evt) {
    mousePosition = evt.GetPosition();
    if (Optional<Size> pointIdx = this->getIdx(mousePosition)) {
        curve.deletePoint(pointIdx.value());
    } else if (Optional<Size> segmentIdx = this->getSegment(mousePosition)) {
        curve.setSegment(segmentIdx.value(), !curve.getSegment(segmentIdx.value()));
    }
    this->Refresh();
}

template <typename TPoint>
TPoint CurvePanel::curveToWindow(const CurvePoint& p) const {
    wxSize size = this->GetSize() - wxSize(2 * padding, 2 * padding);
    const Interval rangeX = curve.rangeX();
    const Interval rangeY = curve.rangeY();

    return TPoint(padding + (p.x - rangeX.lower()) / rangeX.size() * size.x,
        padding + size.y - (p.y - rangeY.lower()) / rangeY.size() * size.y);
}

CurvePoint CurvePanel::windowToCurve(const wxPoint2DDouble p) const {
    wxSize size = this->GetSize() - wxSize(2 * padding, 2 * padding);
    const Interval rangeX = curve.rangeX();
    const Interval rangeY = curve.rangeY();
    return CurvePoint{ float((p.m_x - padding) * rangeX.size() / size.x + rangeX.lower()),
        float(rangeY.size() - (p.m_y - padding) * rangeY.size() / size.y + rangeY.lower()) };
}

Optional<Size> CurvePanel::getIdx(const wxPoint mousePos) const {
    for (Size i = 0; i < curve.getPointCnt(); ++i) {
        wxPoint dist = curveToWindow<wxPoint>(curve.getPoint(i)) - mousePos;
        if (sqr(dist.x) + sqr(dist.y) < sqr(radius)) {
            return i;
        }
    }
    return NOTHING;
}

Optional<Size> CurvePanel::getSegment(const wxPoint mousePos) const {
    for (Size i = 0; i < curve.getPointCnt() - 1; ++i) {
        const wxPoint p1 = curveToWindow<wxPoint>(curve.getPoint(i));
        const wxPoint p2 = curveToWindow<wxPoint>(curve.getPoint(i + 1));
        if (mousePos.x > p1.x && mousePos.x < p2.x) {
            CurvePoint m = windowToCurve(mousePos);
            m.y = curve(m.x);
            const wxPoint projPos = curveToWindow<wxPoint>(m);
            if (abs(mousePos.y - projPos.y) < radius) {
                return i;
            }
        }
    }
    return NOTHING;
}

wxPGWindowList CurveEditor::CreateControls(wxPropertyGrid* propgrid,
    wxPGProperty* property,
    const wxPoint& pos,
    const wxSize& size) const {
    (void)propgrid;
    (void)property;
    (void)pos;
    (void)size;
    CurveProperty* curve = dynamic_cast<CurveProperty*>(property);
    CurveDialog* dialog =
        new CurveDialog(propgrid, curve->getCurve(), [propgrid, property](const Curve& curve) {
            wxPropertyGridEvent* changeEvent = new wxPropertyGridEvent(wxEVT_PG_CHANGED);
            changeEvent->SetProperty(property);
            CurveProperty* curveProp = dynamic_cast<CurveProperty*>(property);
            ASSERT(curveProp);
            curveProp->setCurve(curve);
            propgrid->GetEventHandler()->ProcessEvent(*changeEvent);
        });
    dialog->Show();
    // wxPGWindowList list(new wxWindow(propgrid, wxID_ANY), new CurvePanel(propgrid));
    return {};
}

void CurveEditor::UpdateControl(wxPGProperty* property, wxWindow* ctrl) const {
    (void)property;
    (void)ctrl;
}

void CurveEditor::DrawValue(wxDC& dc,
    const wxRect& rect,
    wxPGProperty* property,
    const wxString& text) const {
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle({ 0, 0 }, { 200, 100 });
    (void)rect;
    (void)property;
    (void)text;
}

bool CurveEditor::OnEvent(wxPropertyGrid* propgrid,
    wxPGProperty* property,
    wxWindow* wnd_primary,
    wxEvent& event) const {
    (void)propgrid;
    (void)property;
    (void)wnd_primary;
    (void)event;
    return true;
}

CurveDialog::CurveDialog(wxWindow* parent, const Curve& curve, Function<void(const Curve&)> curveChanged)
    : wxFrame(parent, wxID_ANY, "Curve", wxDefaultPosition, wxSize(600, 450))
    , curveChanged(curveChanged) {
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    CurvePanel* panel = new CurvePanel(this);
    panel->setCurve(curve);
    sizer->Add(panel, 1, wxEXPAND | wxALL);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* okButton = new wxButton(this, wxID_ANY, "OK");
    okButton->Bind(wxEVT_BUTTON, [this, panel](wxCommandEvent& UNUSED(evt)) {
        this->curveChanged(panel->getCurve());
        this->Close();
    });
    buttonSizer->Add(okButton, 0);

    wxButton* cancelButton = new wxButton(this, wxID_ANY, "Cancel");
    cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->Close(); });
    buttonSizer->Add(cancelButton, 0);

    sizer->Add(buttonSizer);

    this->SetSizer(sizer);
    this->Layout();
}

NAMESPACE_SPH_END
