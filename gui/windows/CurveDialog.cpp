#include "gui/windows/CurveDialog.h"
#include "gui/Utils.h"
#include "post/Plot.h"
#include "post/Point.h"
#include <iostream>
#include <wx/aui/framemanager.h>
#include <wx/graphics.h>

NAMESPACE_SPH_BEGIN

CurvePanel::CurvePanel(wxWindow* parent,
    const Interval& rangeX,
    const Interval& rangeY,
    const Function<Float(Float)>& ticsFuncX)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(800, 200))
    , rangeX(rangeX)
    , rangeY(rangeY)
    , ticsFuncX(ticsFuncX) {
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
            const float f = float(curve(windowToCurve(wxPoint(x, 0)).x));
            const float y = float(curveToWindow<wxPoint2DDouble, double>(CurvePoint{ 0.f, f }).m_y);
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
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(wxPoint(padding, padding), size);
    /*padding + size.y), wxPoint(padding + size.x, padding + size.y));
    dc.DrawLine(wxPoint(padding, padding), wxPoint(padding, padding + size.y));*/

    // draw tics
    const Interval rangeX = this->getRangeX();
    Array<Float> ticsX = getLinearTics(rangeX, 4);
    for (Size i = 0; i < ticsX.size(); ++i) {
        const String label = toPrintableString(ticsX[i], 1);
        const int x = padding + i * size.x / (ticsX.size() - 1);
        drawTextWithSubscripts(dc, label, wxPoint(x - 6, size.y + padding + 6));
        dc.DrawLine(wxPoint(x, size.y + padding - 2), wxPoint(x, size.y + padding + 2));
    }
    /*const Interval rangeY = curve.rangeY();
    Array<Float> ticsY = getLinearTics(rangeY, 3);
    for (Size i = 0; i < ticsY.size(); ++i) {
        const String label = toPrintableString(ticsY[i], 1);
        const int y = padding + size.y - i * size.y / (ticsY.size() - 1);
        drawTextWithSubscripts(dc, label, wxPoint(2, y - 8));
        dc.DrawLine(wxPoint(padding - 2, y), wxPoint(padding + 2, y));
    }*/

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
        const String labelX = toPrintableString(ticsFuncX(curvePos.x), 2);
        const String labelY = toPrintableString(curvePos.y, 2);
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
        Size idx = lockedIdx.value();
        if (idx == 0 || idx == curve.getPointCnt() - 1) {
            newPos.x = curve.getPoint(idx).x;
        }
        curve.setPoint(idx, this->clamp(newPos));
        onCurveChanged.callIfNotNull(curve);
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
        if (newPos.x > 0.f && newPos.x < 1.f) {
            curve.addPoint(newPos);
            onCurveChanged.callIfNotNull(curve);
            lockedIdx = this->getIdx(mousePosition);
        }
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
    onCurveChanged.callIfNotNull(curve);
    this->Refresh();
}

Interval CurvePanel::getRangeX() const {
    return rangeX;
}

Interval CurvePanel::getRangeY() const {
    return rangeY;
}

CurvePoint CurvePanel::clamp(const CurvePoint& p) const {
    return CurvePoint{ rangeX.clamp(p.x), rangeY.clamp(p.y) };
}

template <typename TPoint, typename T>
TPoint CurvePanel::curveToWindow(const CurvePoint& p) const {
    wxSize size = this->GetSize() - wxSize(2 * padding, 2 * padding);
    const Interval rangeX = this->getRangeX();
    const Interval rangeY = this->getRangeY();

    return TPoint(T(padding + (p.x - rangeX.lower()) / rangeX.size() * size.x),
        T(padding + size.y - (p.y - rangeY.lower()) / rangeY.size() * size.y));
}

CurvePoint CurvePanel::windowToCurve(const wxPoint2DDouble p) const {
    wxSize size = this->GetSize() - wxSize(2 * padding, 2 * padding);
    const Interval rangeX = this->getRangeX();
    const Interval rangeY = this->getRangeY();
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

static wxPoint curveToWindow(const Curve& curve, const wxSize size, const CurvePoint& p) {
    const Interval rangeX = curve.rangeX();
    const Interval rangeY = curve.rangeY();

    return wxPoint((p.x - rangeX.lower()) / rangeX.size() * size.x,
        size.y - (p.y - rangeY.lower()) / rangeY.size() * size.y);
}

static CurvePoint windowToCurve(const Curve& curve, const wxSize size, const wxPoint& p) {
    const Interval rangeX = curve.rangeX();
    const Interval rangeY = curve.rangeY();
    return CurvePoint{ float(p.x * rangeX.size() / size.x + rangeX.lower()),
        float(rangeY.size() - p.y * rangeY.size() / size.y + rangeY.lower()) };
}

void CurvePreview::onPaint(wxPaintEvent& UNUSED(evt)) {
    wxPaintDC dc(this);
    this->draw(dc, curve, wxRect(wxPoint(0, 0), this->GetSize()));
}

void CurvePreview::draw(wxDC& dc, const Curve& curve, const wxRect rect) {
    std::cout << "Redrawing ... " << std::endl;
    wxPen pen = *wxWHITE_PEN;
    pen.SetWidth(2);
    dc.SetPen(pen);

    wxSize size = rect.GetSize();
    wxPoint p0 = wxDefaultPosition;
    for (Size i = 0; i < curve.getPointCnt() - 1; ++i) {
        const int x1 = curveToWindow(curve, size, curve.getPoint(i)).x;
        const int x2 = curveToWindow(curve, size, curve.getPoint(i + 1)).x;

        for (int x = x1; x <= x2; ++x) {
            const float f = float(curve(windowToCurve(curve, size, wxPoint(x, 0)).x));
            const float y = float(curveToWindow(curve, size, CurvePoint{ 0.f, f }).y);
            wxPoint p = wxPoint(x, y) + rect.GetPosition();
            if (p0 != wxDefaultPosition) {
                dc.DrawLine(p0, p);
            }
            p0 = p;
        }
    }
}

wxPGWindowList CurvePgEditor::CreateControls(wxPropertyGrid* propgrid,
    wxPGProperty* property,
    const wxPoint& pos,
    const wxSize& size) const {
    CurveProperty* curveProp = dynamic_cast<CurveProperty*>(property);
    SPH_ASSERT(curveProp);

    CurvePanel* panel = new CurvePanel(propgrid->GetParent());
    panel->setCurve(curveProp->getCurve());

    wxAuiPaneInfo info;
    info.Left()
        .MinSize(wxSize(300, -1))
        .Position(1)
        .CaptionVisible(true)
        .DockFixed(false)
        .CloseButton(true)
        .DestroyOnClose(true)
        .Caption("Palette");
    aui->AddPane(panel, info);
    aui->Update();

    CurvePreview* preview = new CurvePreview(propgrid, pos, size, curveProp->getCurve());

    panel->setCurveChangedCallback([=](const Curve& curve) {
        curveProp->setCurve(curve);
        preview->setCurve(curve);
        // propgrid->Refresh();
    });

    return wxPGWindowList(preview);
}

void CurvePgEditor::UpdateControl(wxPGProperty* property, wxWindow* ctrl) const {
    (void)property;
    (void)ctrl;
}

void CurvePgEditor::DrawValue(wxDC& dc,
    const wxRect& rect,
    wxPGProperty* property,
    const wxString& UNUSED(text)) const {
    CurveProperty* curveProp = dynamic_cast<CurveProperty*>(property);
    SPH_ASSERT(curveProp);

    CurvePreview::draw(dc, curveProp->getCurve(), rect);
    //    dc.SetBrush(*wxBLACK_BRUSH);
    //  dc.DrawRectangle(rect);

    (void)rect;
    (void)property;
}

bool CurvePgEditor::OnEvent(wxPropertyGrid* propgrid,
    wxPGProperty* property,
    wxWindow* wnd_primary,
    wxEvent& event) const {
    (void)propgrid;
    (void)property;
    (void)wnd_primary;
    (void)event;
    /*if (wnd_primary) {
        wnd_primary->Update();
    }*/
    return false;
}


NAMESPACE_SPH_END
