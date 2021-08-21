#pragma once

#include "system/Statistics.h"
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class ProgressPanel : public wxPanel {
private:
    String name;

    struct {
        float progress = 0.f;
        String simulationTime;
        String eta;
        bool finished = false;
    } stat;

public:
    explicit ProgressPanel(wxWindow* parent);

    void onRunStart(const String& className, const String& instanceName);

    void onRunEnd();

    void update(const Statistics& stats);

    void reset();

    void onPaint(wxPaintEvent& evt);
};

NAMESPACE_SPH_END
