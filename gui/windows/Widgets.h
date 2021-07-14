#pragma once

#include "objects/wrappers/Function.h"
#include "objects/wrappers/Interval.h"
#include <wx/combobox.h>
#include <wx/textctrl.h>

NAMESPACE_SPH_BEGIN

class FloatTextCtrl : public wxTextCtrl {
private:
    double value;
    double lastValidValue;
    Interval range;

public:
    Function<bool(double)> onValueChanged;

    FloatTextCtrl(wxWindow* parent, const double value, const Interval range = Interval::unbounded());

    double getValue() const {
        return value;
    }

private:
    void validate();
};

class ComboBox : public wxComboBox {
public:
    ComboBox(wxWindow* parent, const wxString& title, const wxSize& size = { -1, -1 })
        : wxComboBox(parent, wxID_ANY, title, wxDefaultPosition, size, {}, wxCB_READONLY) {}
};

NAMESPACE_SPH_END
