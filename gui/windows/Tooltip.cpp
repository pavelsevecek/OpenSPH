#include "gui/windows/Tooltip.h"
#include <wx/sizer.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

Tooltip::Tooltip(wxWindow* parent, wxPoint position, const String& text)
    : wxPopupWindow(parent) {
    wxPoint screenPosition = parent->ClientToScreen(position) + wxPoint(10, 10);
    this->SetPosition(screenPosition);
    this->SetSize(wxSize(500, -1));
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxStaticText* content = new wxStaticText(this, wxID_ANY, text.toUnicode());

    sizer->Add(content, 0, wxEXPAND | wxALL, 8);
    this->SetSizerAndFit(sizer);
}

NAMESPACE_SPH_END
