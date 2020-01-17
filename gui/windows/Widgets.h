#pragma once

#include "objects/wrappers/Function.h"
#include "objects/wrappers/Interval.h"
#include <wx/textctrl.h>

NAMESPACE_SPH_BEGIN

class FloatTextCtrl : public wxTextCtrl {
private:
    double value;
    double lastValidValue;
    Interval range;

public:
    Function<void(double)> onValueChanged;

    FloatTextCtrl(wxWindow* parent, const double value, const Interval range = Interval::unbounded());

    double getValue() const {
        return value;
    }

private:
    void validate();
};

NAMESPACE_SPH_END
