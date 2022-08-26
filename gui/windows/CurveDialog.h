#pragma once

// for some reason this is missing in wx headers
class wxBitmapBundle;

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

NAMESPACE_SPH_BEGIN

class CurvePanel : public wxPanel {
private:
    Curve curve;

    wxPoint mousePosition = wxDefaultPosition;
    Optional<Size> lockedIdx;
    Optional<Size> highlightIdx;
    Optional<Size> highlightSegment;

public:
    CurvePanel(wxWindow* parent);

    void setCurve(const Curve& newCurve) {
        curve = newCurve;
    }

    Curve getCurve() const {
        return curve;
    }

private:
    void onPaint(wxPaintEvent& evt);

    void onLeftDown(wxMouseEvent& evt);

    void onLeftUp(wxMouseEvent& evt);

    void onRightUp(wxMouseEvent& evt);

    void onMouseMotion(wxMouseEvent& evt);

    const static int padding = 30;

    template <typename TPoint, typename T = int>
    TPoint curveToWindow(const CurvePoint& p) const;

    CurvePoint windowToCurve(const wxPoint2DDouble p) const;

    Optional<Size> getIdx(const wxPoint mousePos) const;

    Optional<Size> getSegment(const wxPoint mousePos) const;
};

class CurveEditor : public wxPGEditor {
public:
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

class CurveProperty : public wxStringProperty {
private:
    Curve curve;

public:
    CurveProperty(const String& label, const Curve& curve)
        : wxStringProperty(label.toUnicode(), "curve")
        , curve(curve) {}

    virtual const wxPGEditor* DoGetEditorClass() const override {
        static wxPGEditor* editor = wxPropertyGrid::RegisterEditorClass(new CurveEditor(), "MyEditor");
        return editor;
    }

    void setCurve(const Curve& newCurve) {
        curve = newCurve;
    }

    const Curve& getCurve() const {
        return curve;
    }
};


class CurveDialog : public wxFrame {
private:
    Function<void(const Curve&)> curveChanged;

public:
    CurveDialog(wxWindow* parent, const Curve& curve, Function<void(const Curve&)> curveChanged);
};

NAMESPACE_SPH_END
