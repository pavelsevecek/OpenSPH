#include "gui/windows/Widgets.h"
#include "objects/utility/StringUtils.h"
#include "thread/CheckFunction.h"
#include <wx/msgdlg.h>
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/props.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

NAMESPACE_SPH_BEGIN

FloatTextCtrl::FloatTextCtrl(wxWindow* parent, const double value, const Interval range)
    : value(value)
    , range(range) {

    lastValidValue = value;

    wxValidator* validator = wxFloatProperty::GetClassValidator();
    this->Create(parent,
        wxID_ANY,
        std::to_string(value),
        wxDefaultPosition,
        wxSize(100, 25),
        wxTE_PROCESS_ENTER | wxTE_RIGHT,
        *validator);

    this->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& evt) {
        this->validate();
        evt.Skip();
    });
    this->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& evt) {
        this->validate();
        evt.Skip();
    });

    this->validate();
}

void FloatTextCtrl::setValue(double newValue) {
    value = range.clamp(newValue);
    lastValidValue = value;

    wxFloatProperty prop;
    wxVariant variant(value);
    this->ChangeValue(prop.ValueToString(variant));
}

void FloatTextCtrl::validate() {
    wxFloatProperty prop;
    wxVariant variant = this->GetValue();
    wxPGValidationInfo info;
    if (!prop.ValidateValue(variant, info)) {
        value = lastValidValue;
    } else {
        value = float(variant.GetDouble());
    }

    value = range.clamp(value);

    if (onValueChanged && value != lastValidValue) {
        const bool validated = onValueChanged(value);
        if (!validated) {
            value = lastValidValue;
        }
    }

    lastValidValue = value;
    variant = double(value);
    this->ChangeValue(prop.ValueToString(variant));
}

class WaitDialog : public wxDialog {
public:
    WaitDialog(wxWindow* parent, const std::string& message)
        : wxDialog(parent, wxID_ANY, "Info", wxDefaultPosition, wxDefaultSize, wxCAPTION | wxSYSTEM_MENU) {
        const wxSize size = wxSize(320, 90);
        this->SetSize(size);
        wxStaticText* text = new wxStaticText(this, wxID_ANY, message);
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->AddStretchSpacer();
        sizer->Add(text, 1, wxALIGN_CENTER_HORIZONTAL);
        sizer->AddStretchSpacer();
        this->SetSizer(sizer);
        this->Layout();
        this->CentreOnScreen();
    }
};

ClosablePage::ClosablePage(wxWindow* parent, const std::string& label)
    : wxPanel(parent, wxID_ANY)
    , label(label) {}

bool ClosablePage::close() {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
    if (this->isRunning()) {
        const int retval = wxMessageBox(
            capitalize(label) + " is currently in progress. Do you want to stop it and close the window?",
            "Stop?",
            wxYES_NO | wxCENTRE);
        if (retval == wxYES) {
            this->stop();
            dialog = new WaitDialog(this, "Waiting for " + label + " to finish ...");
            dialog->ShowModal();
            this->quit();
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }
}

void ClosablePage::onStopped() {
    if (dialog) {
        dialog->EndModal(0);
    }
}


NAMESPACE_SPH_END
