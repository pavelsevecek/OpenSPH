#include "gui/windows/SessionDialog.h"
#include "run/jobs/Presets.h"
#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

SessionDialog::SessionDialog(wxWindow* parent, UniqueNameManager& nameMgr)
    : wxDialog(parent, wxID_ANY, "New session", wxDefaultPosition, wxSize(500, 400)) {

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, "New session:"));

    wxRadioButton* emptyButton =
        new wxRadioButton(this, wxID_ANY, "Empty session", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    sizer->Add(emptyButton);

    wxRadioButton* presetButton = new wxRadioButton(this, wxID_ANY, "Select a preset:");
    sizer->Add(presetButton);

    wxArrayString options;
    for (Presets::Id id : EnumMap::getAll<Presets::Id>()) {
        std::string name = replaceAll(EnumMap::toString(id), "_", " ");
        options.Add(name);
    }
    int height = this->GetClientSize().y - 100;
    wxListBox* list =
        new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(450, height), options, wxLB_SINGLE);
    list->Enable(false);
    sizer->Add(list, 0, wxALIGN_CENTER_HORIZONTAL);

    emptyButton->Bind(wxEVT_RADIOBUTTON, [list](wxCommandEvent&) { list->Enable(false); });
    presetButton->Bind(wxEVT_RADIOBUTTON, [list](wxCommandEvent&) { list->Enable(true); });

    sizer->AddSpacer(5);
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* createButton = new wxButton(this, wxID_ANY, "Create");
    wxButton* cancelButton = new wxButton(this, wxID_ANY, "Cancel");
    buttonSizer->Add(createButton);
    buttonSizer->Add(cancelButton);
    sizer->Add(buttonSizer, 0, wxALIGN_RIGHT);

    auto createSession = [=, &nameMgr](wxCommandEvent& UNUSED(evt)) {
        if (presetButton->GetValue()) {
            int idx = list->GetSelection();
            if (idx == wxNOT_FOUND) {
                wxMessageBox("Select a preset to create", "No preset", wxOK | wxCENTRE);
                return;
            } else {
                Presets::Id id = Presets::Id(idx);
                preset = Presets::make(id, nameMgr);

                /// \todo these presets should also setup GUI parameters (palettes, particle radii, ...) !!
            }
        }
        this->EndModal(wxID_OK);
    };
    list->Bind(wxEVT_LISTBOX_DCLICK, createSession);
    createButton->Bind(wxEVT_BUTTON, createSession);
    cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->EndModal(wxID_CANCEL); });

    this->SetSizer(sizer);
}

SessionDialog::~SessionDialog() = default;

NAMESPACE_SPH_END
