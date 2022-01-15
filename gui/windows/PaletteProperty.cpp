#include "gui/windows/PaletteProperty.h"
#include "gui/objects/PaletteEntry.h"
#include "gui/objects/RenderContext.h"
#include "gui/windows/PaletteWidget.h"
#include <wx/aui/framemanager.h>

NAMESPACE_SPH_BEGIN

wxPGWindowList PalettePgEditor::CreateControls(wxPropertyGrid* propgrid,
    wxPGProperty* property,
    const wxPoint& UNUSED(pos),
    const wxSize& UNUSED(size)) const {
    PaletteProperty* paletteProp = dynamic_cast<PaletteProperty*>(property);
    SPH_ASSERT(paletteProp);
    const static wxString paneName = "PaletteSetup";
    if (aui->GetPane(paneName).IsOk()) {
        // already exists
        return wxPGWindowList(nullptr);
    }

    PaletteAdvancedWidget* panel = nullptr;
    panel = new PaletteAdvancedWidget(propgrid->GetParent(), wxSize(300, 200), paletteProp->getPalette());
    panel->onPaletteChanged = [propgrid, paletteProp](const Palette& palette) { //
        paletteProp->setPalete(propgrid, palette);
    };
    panel->Bind(wxEVT_DESTROY, [propgrid, property](wxWindowDestroyEvent& UNUSED(evt)) { //
        propgrid->RemoveFromSelection(property);
    });

    wxAuiPaneInfo info;
    info.Name(paneName)
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

    return wxPGWindowList(nullptr);
}

void PalettePgEditor::UpdateControl(wxPGProperty* UNUSED(property), wxWindow* UNUSED(ctrl)) const {}

void PalettePgEditor::DrawValue(wxDC& dc,
    const wxRect& rect,
    wxPGProperty* property,
    const wxString& UNUSED(text)) const {
    PaletteProperty* paletteProp = dynamic_cast<PaletteProperty*>(property);
    SPH_ASSERT(paletteProp);

    WxRenderContext context(dc);
    Pixel position(rect.GetPosition());
    Pixel size(rect.GetWidth(), rect.GetHeight());
    drawPalette(context, position, size, paletteProp->getPalette(), NOTHING);
}

bool PalettePgEditor::OnEvent(wxPropertyGrid* UNUSED(propgrid),
    wxPGProperty* UNUSED(property),
    wxWindow* UNUSED(primary),
    wxEvent& UNUSED(event)) const {
    // unused
    return false;
}

PaletteProperty::PaletteProperty(const String& label, const Palette& palette, wxAuiManager* aui)
    : wxStringProperty(label.toUnicode(), "palette")
    , palette(palette)
    , aui(aui) {}

PaletteProperty::~PaletteProperty() {
    wxAuiPaneInfo& info = aui->GetPane("PaletteSetup");
    if (info.IsOk()) {
        aui->ClosePane(info);
    }
}

const wxPGEditor* PaletteProperty::DoGetEditorClass() const {
    static wxPGEditor* editor =
        wxPropertyGrid::DoRegisterEditorClass(new PalettePgEditor(palette, aui), "PaletteEditor");
    return editor;
}

void PaletteProperty::setPalete(wxWindow* parent, const Palette& newPalette) {
    palette = newPalette;

    PaletteEntry entry(palette);
    this->SetValue(entry.toString().toUnicode());
    wxPropertyGridEvent evt(wxEVT_PG_CHANGED);
    evt.SetProperty(this);
    parent->GetEventHandler()->ProcessEvent(evt);
}

NAMESPACE_SPH_END
