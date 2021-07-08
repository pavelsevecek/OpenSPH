#pragma once

#include "gui/Controller.h"
#include "gui/Settings.h"
#include <wx/dialog.h>

class wxComboBox;
class wxCheckBox;
class wxTextCtrl;

NAMESPACE_SPH_BEGIN

class FloatTextCtrl;

class GuiSettingsDialog : public wxDialog {
private:
    wxComboBox* colorizerBox;
    Array<ExtColorizerId> colorizerIds;

    FloatTextCtrl* periodCtrl;
    wxTextCtrl* overplotPath;

    FlatMap<wxCheckBox*, PlotEnum> plotBoxMap;

public:
    GuiSettingsDialog(wxWindow* parent);

    ~GuiSettingsDialog();

private:
    void commit();
};

NAMESPACE_SPH_END
