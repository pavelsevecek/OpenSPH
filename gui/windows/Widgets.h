#pragma once

#include "objects/wrappers/Function.h"
#include "objects/wrappers/Interval.h"
#include <wx/textctrl.h>

NAMESPACE_SPH_BEGIN

class FloatTextCtrl : public wxTextCtrl {
private:
    float value;
    float lastValidValue;
    Interval range;

public:
    Function<void(Float)> onValueChanged;

    FloatTextCtrl(wxWindow* parent, const float value, const Interval range = Interval::unbounded());

    float getValue() const {
        return value;
    }

private:
    void validate();
};

NAMESPACE_SPH_END
