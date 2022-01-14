#pragma once

#include "gui/objects/Color.h"
#include "gui/objects/Palette.h"
#include "objects/containers/Array.h"
#include "objects/utility/Streams.h"
#include "objects/wrappers/Optional.h"
#include "run/SpecialEntries.h"
#include <iostream>
#include <wx/colordlg.h>
#include <wx/dcbuffer.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/propgrid/editors.h>
#include <wx/propgrid/propgrid.h>

class wxAuiManager;

NAMESPACE_SPH_BEGIN

class PaletteEditor : public wxPanel {
private:
    Palette palette;
    // copied-out palette points, must be kept in sync
    Array<Palette::Point> points;

    Optional<Size> active;
    bool enabled = true;

public:
    PaletteEditor(wxWindow* parent, const wxSize size, const Palette& palette);

    void setPalette(const Palette& newPalette);

    void setPaletteColors(const Palette& newPalette);

    const Palette& getPalette() const {
        return palette;
    }

    void enable(const bool value) {
        enabled = value;
        this->Refresh();
    }

    Function<void(const Palette& palette)> onPaletteChanged;
    // Function<void(Array<Palette::Point>&& points)> onPointsChanged;

private:
    void updatePaletteFromPoints();

    void onPaint(wxPaintEvent& evt);

    Optional<Size> lock(const int x) const {
        for (Size i = 0; i < points.size(); ++i) {
            const int p = pointToWindow(points[i].value);
            if (abs(x - p) < 10) {
                return i;
            }
        }
        return NOTHING;
    }

    float windowToPoint(const int x) const;

    int pointToWindow(const float x) const;

    void onMouseMotion(wxMouseEvent& evt);

    void onLeftUp(wxMouseEvent& UNUSED(evt)) {
        active = NOTHING;
    }

    void onLeftDown(wxMouseEvent& evt) {
        active = this->lock(evt.GetPosition().x);
    }

    void onRightUp(wxMouseEvent& evt);

    void onDoubleClick(wxMouseEvent& evt);
};

/*class PaletteEntry : public IExtraEntry {
private:
    Palette palette;

public:
    PaletteEntry() = default;

    PaletteEntry(const Palette& palette)
        : palette(palette) {}

    virtual String toString() const override;

    virtual void fromString(const String& s) override;

    virtual AutoPtr<IExtraEntry> clone() const override {
        return makeAuto<PaletteEntry>(palette);
    }

    const Palette& getPalette() const {
        return palette;
    }
};


class PalettePreview : public wxPanel {
private:
    Palette palette;

public:
    PalettePreview(wxWindow* parent, const wxPoint point, const wxSize size, const Palette& palette);

    void setPalette(const Palette& newPalette) {
        palette = newPalette;
        this->Refresh();
    }

private:
    void onPaint(wxPaintEvent& evt);
};*/

/*class PalettePgEditor : public wxPGEditor {
private:
    Palette palette;
    wxAuiManager* aui;

public:
    PalettePgEditor(const Palette& palette, wxAuiManager* aui)
        : palette(palette)
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


class PaletteProperty : public wxStringProperty {
private:
    Palette palette;
    wxAuiManager* aui;
    wxWindow* parent;

public:
    PaletteProperty(wxWindow* parent, const String& label, const Palette& palette, wxAuiManager* aui)
        : wxStringProperty(label.toUnicode(), "palette")
        , palette(palette)
        , aui(aui)
        , parent(parent) {}

    virtual const wxPGEditor* DoGetEditorClass() const override {
        static wxPGEditor* editor =
            wxPropertyGrid::DoRegisterEditorClass(new PalettePgEditor(palette, aui), "PaletteEditor");
        return editor;
    }

    void setPalete(const Palette& newPalette) {
        palette = newPalette;

        PaletteEntry entry(palette);
        this->SetValue(entry.toString().toUnicode());
        wxPropertyGridEvent evt(wxEVT_PG_CHANGED);
        evt.SetProperty(this);
        parent->GetEventHandler()->ProcessEvent(evt);
    }

    const Palette& getPalette() const {
        return palette;
    }
};
*/
NAMESPACE_SPH_END
