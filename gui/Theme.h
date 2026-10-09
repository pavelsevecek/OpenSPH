#pragma once

#include "common/Globals.h"
#include <wx/aui/aui.h>
#include <wx/grid.h>
#include <wx/propgrid/propgrid.h>
#include <wx/combobox.h>
#include <wx/textctrl.h>
#include <wx/treectrl.h>

NAMESPACE_SPH_BEGIN

namespace DarkTheme {

#ifdef __WXMSW__
inline void applyNative(wxWindow* window);
#endif

inline wxColour background() {
    return wxColour(32, 34, 38);
}

inline wxColour surface() {
    return wxColour(45, 48, 54);
}

inline wxColour text() {
    return wxColour(230, 232, 236);
}

inline wxColour selection() {
    return wxColour(55, 88, 130);
}

inline void apply(wxWindow* window) {
    window->SetBackgroundColour(background());
    window->SetForegroundColour(text());
}

inline void apply(wxPropertyGrid* grid) {
    apply(static_cast<wxWindow*>(grid));
    grid->SetCellBackgroundColour(background());
    grid->SetCellTextColour(text());
    grid->SetCellDisabledTextColour(wxColour(145, 149, 157));
    grid->SetCaptionBackgroundColour(surface());
    grid->SetCaptionTextColour(text());
    grid->SetMarginColour(surface());
    grid->SetEmptySpaceColour(background());
    grid->SetLineColour(surface());
    grid->SetSelectionBackgroundColour(selection());
    grid->SetSelectionTextColour(text());
}

inline void apply(wxGrid* grid) {
    apply(static_cast<wxWindow*>(grid));
    grid->SetDefaultCellBackgroundColour(background());
    grid->SetDefaultCellTextColour(text());
    grid->SetLabelBackgroundColour(surface());
    grid->SetLabelTextColour(text());
    grid->SetGridLineColour(wxColour(65, 69, 77));
    grid->SetSelectionBackground(selection());
    grid->SetSelectionForeground(text());
}

inline void apply(wxAuiManager* manager) {
    wxAuiDockArt* art = manager->GetArtProvider();
    art->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE, wxAUI_GRADIENT_NONE);
    art->SetColour(wxAUI_DOCKART_BACKGROUND_COLOUR, background());
    art->SetColour(wxAUI_DOCKART_SASH_COLOUR, background());
    art->SetColour(wxAUI_DOCKART_BORDER_COLOUR, surface());
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR, surface());
    art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, text());
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR, selection());
    art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR, text());
}

// Apply again when a window is shown, after its children have been constructed.
inline void applyTree(wxWindow* window) {
#ifdef __WXMSW__
    applyNative(window);
#endif
    // These native controls establish their own light defaults during Create().
    if (dynamic_cast<wxTreeCtrl*>(window) || dynamic_cast<wxTextCtrl*>(window) ||
        dynamic_cast<wxComboBox*>(window)) {
        apply(window);
    }
    // Preserve explicit colours used by render previews and palette controls.
    if (!window->UseBackgroundColour()) {
        window->SetBackgroundColour(background());
    }
    if (!window->UseForegroundColour()) {
        window->SetForegroundColour(text());
    }
    for (wxWindow* child : window->GetChildren()) {
        applyTree(child);
    }
}

} // namespace DarkTheme

NAMESPACE_SPH_END

#include "gui/WindowsTheme.h"
