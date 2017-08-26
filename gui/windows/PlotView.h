#pragma once

/// \file PlotView.h
/// \brief Drawing of plots
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/objects/Point.h"
#include "post/Plot.h"
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class PlotView : public wxPanel {
private:
    AutoPtr<Abstract::Plot> plot;
    wxSize padding;

public:
    PlotView(wxWindow* parent, const wxSize size, const wxSize padding)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
        , padding(padding) {
        this->SetMaxSize(size);
        Connect(wxEVT_PAINT, wxPaintEventHandler(PlotView::onPaint));
        // plot = makeAuto<TestPlot>();
    }

private:
    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);
        wxBrush brush;
        brush.SetColour(this->GetBackgroundColour());
        dc.SetBrush(brush);
        dc.DrawRectangle(wxPoint(0, 0), this->GetSize());

        const Interval rangeX = plot->getRangeX();
        const Interval rangeY = plot->getRangeY();
        drawAxes(dc, rangeX, rangeY);
        drawPlot(dc, rangeX, rangeY);
    }

    void drawPlot(wxDC& dc, const Interval rangeX, const Interval rangeY) {
        wxPen pen;
        pen.SetColour(*wxBLUE);
        dc.SetPen(pen);
        Storage storage;
        Array<PlotPoint> points; /// \todo  = plot->plot();
        const wxSize size = this->GetSize();
        const wxSize plotSize = size - 2 * padding;
        for (Size i = 0; i < points.size() - 1; ++i) {
            const int x1(padding.x + plotSize.x * (points[i].x - rangeX.lower()) / rangeX.size());
            const int x2(padding.x + plotSize.x * (points[i + 1].x - rangeX.lower()) / rangeX.size());
            const int y1(padding.y + plotSize.y * (points[i].y - rangeY.lower()) / rangeY.size());
            const int y2(padding.y + plotSize.y * (points[i + 1].y - rangeY.lower()) / rangeY.size());
            dc.DrawLine(x1, size.y - y1, x2, size.y - y2);
        }
    }

    void drawAxes(wxDC& dc, const Interval rangeX, const Interval rangeY) {
        wxPen pen;
        pen.SetColour(*wxWHITE);
        dc.SetPen(pen);
        const wxSize size = this->GetSize();
        dc.DrawLine(padding.x, size.y - padding.y, padding.x, padding.y);
        dc.DrawLine(padding.x, size.y - padding.y, size.x - padding.x, size.y - padding.y);
        (void)rangeX;
        (void)rangeY;
    }
};

NAMESPACE_SPH_END
