#pragma once

#include "objects/wrappers/Function.h"
#include "objects/wrappers/Interval.h"
#include <wx/combobox.h>
#include <wx/panel.h>
#include <wx/textctrl.h>

NAMESPACE_SPH_BEGIN

class WaitDialog;

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

    void setValue(double newValue);

private:
    void validate();
};

class ComboBox : public wxComboBox {
public:
    ComboBox(wxWindow* parent, const wxString& title, const int width = -1)
        : wxComboBox(parent, wxID_ANY, title, wxDefaultPosition, wxSize(width, 27), {}, wxCB_READONLY) {}
};

class ClosablePage : public wxPanel {
private:
    std::string label;
    WaitDialog* dialog = nullptr;

public:
    ClosablePage(wxWindow* parent, const std::string& label);

    // false means close has been veto'd
    bool close();

protected:
    void onStopped();

private:
    virtual bool isRunning() const = 0;
    virtual void stop() = 0;
    virtual void quit() = 0;
};


NAMESPACE_SPH_END
