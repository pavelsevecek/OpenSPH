#pragma once

#include "gui/objects/Color.h"
#include "gui/objects/Point.h"
#include "math/Curve.h"
#include "objects/containers/Array.h"
#include "objects/containers/String.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/Optional.h"
#include <algorithm>
#include <wx/dcclient.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/propgrid/editors.h>
#include <wx/propgrid/propgrid.h>
#include <wx/sizer.h>

class wxAuiManager;

NAMESPACE_SPH_BEGIN

class CurvePanel : public wxPanel {
private:
    Curve curve;
    Interval rangeX;
    Interval rangeY;

    Function<Float(Float)> ticsFuncX;

    wxPoint mousePosition = wxDefaultPosition;
    Optional<Size> lockedIdx;
    Optional<Size> highlightIdx;
    Optional<Size> highlightSegment;

    Function<void(const Curve& curve)> onCurveChanged;

public:
    CurvePanel(
        wxWindow* parent,
        const Interval& rangeX = Interval(0, 1),
        const Interval& rangeY = Interval(0, 1),
        const Function<Float(Float)>& ticsFuncX = [](Float x) { return x; });

    void setCurve(const Curve& newCurve) {
        curve = newCurve;
    }

    Curve getCurve() const {
        return curve;
    }

    void setCurveChangedCallback(Function<void(const Curve& curve)> callback) {
        onCurveChanged = callback;
    }

private:
    void onPaint(wxPaintEvent& evt);

    void onLeftDown(wxMouseEvent& evt);

    void onLeftUp(wxMouseEvent& evt);

    void onRightUp(wxMouseEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    const static int padding = 10;

    Interval getRangeX() const;
    Interval getRangeY() const;

    CurvePoint clamp(const CurvePoint& p) const;

    template <typename TPoint, typename T = int>
    TPoint curveToWindow(const CurvePoint& p) const;

    CurvePoint windowToCurve(const wxPoint2DDouble p) const;

    Optional<Size> getIdx(const wxPoint mousePos) const;

    Optional<Size> getSegment(const wxPoint mousePos) const;
};

class CurvePreview : public wxPanel {
private:
    Curve curve;

public:
    CurvePreview(wxWindow* parent, wxPoint position, wxSize size, const Curve& curve)
        : wxPanel(parent, wxPG_SUBID1, position, size)
        , curve(curve) {
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(CurvePreview::onPaint));
    }

    void setCurve(const Curve& newCurve) {
        curve = newCurve;
        this->Refresh();
    }

    static void draw(wxDC& dc, const Curve& curve, const wxRect size);

private:
    void onPaint(wxPaintEvent& evt);
};

class CurvePgEditor : public wxPGEditor {
private:
    Curve curve;
    wxAuiManager* aui;

public:
    CurvePgEditor(const Curve& curve, wxAuiManager* aui)
        : curve(curve)
        , aui(aui) {}


    virtual wxPGWindowList CreateControls(wxPropertyGrid* propgrid,
        wxPGProperty* property,
        const wxPoint& pos,
        const wxSize& size) const override;

    virtual void UpdateControl(wxPGProperty* property, wxWindow* ctrl) const override;

    virtual void DrawValue(wxDC& dc,
        const wxRect& rect,
        wxPGProperty* property,
        const wxString& text) const override;

    virtual bool OnEvent(wxPropertyGrid* propgrid,
        wxPGProperty* property,
        wxWindow* wnd_primary,
        wxEvent& event) const override;
};

class CurveProperty : public wxPGProperty {
private:
    Curve curve;
    wxAuiManager* aui;

public:
    CurveProperty(const String& label, const Curve& curve, wxAuiManager* aui)
        : wxPGProperty(label.toUnicode(), "curve")
        , curve(curve)
        , aui(aui) {}

    virtual const wxPGEditor* DoGetEditorClass() const override {
        static wxPGEditor* editor =
            wxPropertyGrid::DoRegisterEditorClass(new CurvePgEditor(curve, aui), wxString("CurveEditor"));
        return editor;
    }

    void setCurve(const Curve& newCurve) {
        curve = newCurve;
    }

    const Curve& getCurve() const {
        return curve;
    }
};

NAMESPACE_SPH_END
