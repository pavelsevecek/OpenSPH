#include "gui/windows/Tooltip.h"
#include <wx/display.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

Tooltip::Tooltip(wxWindow* parent, wxPoint position, const String& text)
    : wxPopupWindow(parent) {
    this->SetSize(wxSize(500, -1));
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* content = new wxStaticText(this, wxID_ANY, text.toUnicode());

    sizer->Add(content, 0, wxEXPAND | wxALL, 8);
    this->SetSizerAndFit(sizer);

    wxPoint screenPosition = parent->ClientToScreen(position) + wxPoint(10, 10);
    wxDisplay display(wxDisplay::GetFromWindow(parent));
    wxRect screen = display.GetClientArea();
    wxSize size = this->GetSize();
    if (screenPosition.x + size.x > screen.GetWidth()) {
        screenPosition.x = screen.GetWidth() - size.x;
    }
    this->SetPosition(screenPosition);
}

NAMESPACE_SPH_END
