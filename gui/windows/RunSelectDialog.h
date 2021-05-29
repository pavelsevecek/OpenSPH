#pragma once

#include "run/Node.h"
#include <wx/dialog.h>

class wxCheckBox;

NAMESPACE_SPH_BEGIN

class RunSelectDialog : public wxDialog {
private:
    Array<SharedPtr<JobNode>> nodes;
    SharedPtr<JobNode> selected;
    wxCheckBox* rememberBox;

public:
    RunSelectDialog(wxWindow* parent, Array<SharedPtr<JobNode>>&& nodes);

    SharedPtr<JobNode> selectedNode() const {
        return selected;
    }

    bool remember() const;

    ~RunSelectDialog();

private:
    void select(const int index);
};

NAMESPACE_SPH_END
