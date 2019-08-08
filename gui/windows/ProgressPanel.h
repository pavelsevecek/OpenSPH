#pragma once

#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class ProgressPanel : public wxPanel {
private:
    std::string name;

    struct {
        float progress = 0._f;
        std::string simulationTime;
        std::string eta;
    } stat;

public:
    ProgressPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY) {
        this->Connect(wxEVT_PAINT, wxPaintEventHandler(ProgressPanel::onPaint));
    }

    void onRunStart(const std::string& className, const std::string& instanceName) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
        name = instanceName + " (" + className + ")";
        this->reset();
        this->Refresh();
    }

    void update(const Statistics& stats) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD | CheckFunction::NO_THROW);
        this->reset();

        stat.progress = stats.getOr<Float>(StatisticsId::RELATIVE_PROGRESS, 0._f);

        if (stats.has(StatisticsId::WALLCLOCK_TIME)) {
            const int64_t wallclock = stats.get<int>(StatisticsId::WALLCLOCK_TIME);
            stat.simulationTime = "Elapsed time: " + getFormattedTime(wallclock);

            if (stat.progress > 0.05_f) {
                stat.eta =
                    "Estimated remaining: " + getFormattedTime(wallclock * (1._f / stat.progress - 1._f));
            }
        }

        this->Refresh();
    }

private:
    void reset() {
        stat.progress = 0._f;
        stat.eta = "";
        stat.simulationTime = "";
    }

    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);
        wxSize size = dc.GetSize();
        constexpr int padding = 25;
        wxRect rect(wxPoint(padding, 0), wxSize(size.x - 2 * padding, size.y));

        wxBrush brush = *wxBLACK_BRUSH;
        const bool isLightTheme = Rgba(dc.GetBackground().GetColour()).intensity() > 0.5f;
        brush.SetColour(isLightTheme ? wxColour(160, 160, 200) : wxColour(100, 100, 120));
        dc.SetBrush(brush);
        dc.DrawRectangle(wxPoint(0, 0), wxSize(int(stat.progress * size.x), size.y));

        wxFont font = dc.GetFont();
        dc.SetFont(font.Bold());
        dc.DrawLabel(name, rect, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);

        dc.SetFont(font);
        dc.DrawLabel(stat.eta, rect, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
        dc.DrawLabel(stat.simulationTime, rect, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL);
    }
};

NAMESPACE_SPH_END
