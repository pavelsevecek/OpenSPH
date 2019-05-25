#include "gui/windows/Widgets.h"
#include <wx/propgrid/propgrid.h>
#include <wx/propgrid/props.h>

NAMESPACE_SPH_BEGIN

FloatTextCtrl::FloatTextCtrl(wxWindow* parent, const float value, const Interval range)
    : value(value)
    , range(range) {

    lastValidValue = value;

    wxValidator* validator = wxFloatProperty::GetClassValidator();
    this->Create(parent,
        wxID_ANY,
        std::to_string(value),
        wxDefaultPosition,
        wxSize(120, 30),
        wxTE_PROCESS_ENTER | wxTE_RIGHT,
        *validator);

    this->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent& UNUSED(evt)) { this->validate(); });
    this->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& UNUSED(evt)) { this->validate(); });

    this->validate();
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
    lastValidValue = value = range.clamp(value);

    variant = double(value);
    this->ChangeValue(prop.ValueToString(variant));

    if (onValueChanged) {
        onValueChanged(value);
    }
}

NAMESPACE_SPH_END
