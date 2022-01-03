#include "gui/windows/RunSelectDialog.h"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

RunSelectDialog::RunSelectDialog(wxWindow* parent, Array<SharedPtr<JobNode>>&& nodes, const String& label)
    : wxDialog(parent, wxID_ANY, "Select run", wxDefaultPosition, wxSize(800, 500))
    , nodes(std::move(nodes)) {

    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(this, wxID_ANY, wxString("Select ") + label.toUnicode() + ":"));
    const int listHeight = this->GetClientSize().y - 70;
    wxListCtrl* list = new wxListCtrl(
        this, wxID_ANY, wxDefaultPosition, wxSize(800, listHeight), wxLC_REPORT | wxLC_SINGLE_SEL);
    const int columnWidth = list->GetSize().x / 2 - 5;
    list->AppendColumn("Name");
    list->AppendColumn("Type");
    list->SetColumnWidth(0, columnWidth);
    list->SetColumnWidth(1, columnWidth);
    int index = 0;
    for (auto& node : this->nodes) {
        wxListItem item;
        item.SetId(index);
        item.SetText(node->instanceName().toUnicode());
        item.SetColumn(0);
        list->InsertItem(item);

        list->SetItem(index, 1, node->className().toUnicode());

        ++index;
    }
    list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& evt) { select(evt.GetIndex()); });
    sizer->Add(list);
    rememberBox = new wxCheckBox(this, wxID_ANY, "Remember choice");
    sizer->Add(rememberBox);

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* runButton = new wxButton(this, wxID_ANY, capitalize(label).toUnicode());
    runButton->Bind(wxEVT_BUTTON, [this, list](wxCommandEvent& UNUSED(evt)) {
        int item = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (item == -1) {
            wxMessageBox("No run selected", "No run", wxOK | wxCENTRE);
            return;
        }
        select(item);
    });
    wxButton* cancelButton = new wxButton(this, wxID_ANY, "Cancel");
    cancelButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent& UNUSED(evt)) { this->EndModal(wxID_CANCEL); });
    buttonSizer->Add(runButton);
    buttonSizer->Add(cancelButton);
    sizer->Add(buttonSizer, 0, wxALIGN_RIGHT);

    this->SetSizer(sizer);
}

RunSelectDialog::~RunSelectDialog() = default;

bool RunSelectDialog::remember() const {
    return rememberBox->GetValue();
}

void RunSelectDialog::select(const int index) {
    selected = nodes[index];
    this->EndModal(wxID_OK);
}

NAMESPACE_SPH_END
