#pragma once

#include "run/Node.h"
#include <wx/dialog.h>

NAMESPACE_SPH_BEGIN

class SessionDialog : public wxDialog {
private:
    SharedPtr<JobNode> preset;

public:
    SessionDialog(wxWindow* parent, UniqueNameManager& nameMgr);

    SharedPtr<JobNode> selectedPreset() const {
        return preset;
    }

    ~SessionDialog();
};

NAMESPACE_SPH_END
