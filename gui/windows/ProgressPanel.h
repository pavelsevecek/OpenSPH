#pragma once

#include "system/Statistics.h"
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class ProgressPanel : public wxPanel {
private:
    std::string name;

    struct {
        float progress = 0.f;
        std::string simulationTime;
        std::string eta;
        bool finished = false;
    } stat;

public:
    explicit ProgressPanel(wxWindow* parent);

    void onRunStart(const std::string& className, const std::string& instanceName);

    void onRunEnd();

    void update(const Statistics& stats);

    void reset();

    void onPaint(wxPaintEvent& evt);
};

NAMESPACE_SPH_END
