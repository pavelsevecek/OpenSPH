#pragma once

#include "gui/objects/Palette.h"
#include <wx/window.h>
// must be after window.h
#include <wx/propgrid/editors.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/props.h>

class wxAuiManager;

NAMESPACE_SPH_BEGIN

class PalettePgEditor : public wxPGEditor {
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

public:
    PaletteProperty(const String& label, const Palette& palette, wxAuiManager* aui);

    ~PaletteProperty();

    virtual const wxPGEditor* DoGetEditorClass() const override;

    void setPalete(wxWindow* parent, const Palette& newPalette);

    const Palette& getPalette() const {
        return palette;
    }
};


NAMESPACE_SPH_END
