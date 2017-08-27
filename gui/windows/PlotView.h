#pragma once

/// \file PlotView.h
/// \brief Drawing of plots
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/objects/Drawing.h"
#include "gui/objects/Point.h"
#include "objects/wrappers/LockingPtr.h"
#include "physics/Integrals.h"
#include "post/Plot.h"
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

class PlotView : public wxPanel {
private:
    LockingPtr<IPlot> plot;
    wxSize padding;
    Color color;
    std::string name;

public:
    PlotView(wxWindow* parent,
        const wxSize size,
        const wxSize padding,
        IntegralWrapper&& integral,
        const Color color)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
        , padding(padding)
        , color(color) {
        this->SetMaxSize(size);
        Connect(wxEVT_PAINT, wxPaintEventHandler(PlotView::onPaint));

        name = integral.getName();

        // plot needs to be synchronized as it is updated from different thread, hopefully neither updating
        // nor drawing will take a lot of time, so we can simply lock the pointer.
        plot = makeLocking<TemporalPlot>(std::move(integral),
            TemporalPlot::Params{ 1._f, Interval{ -10._f, 10._f }, 1.e4_f, false, 0.05_f });
    }

    /// Update the plot data on time step, can be done from any thread
    void onTimeStep(const Storage& storage, const Statistics& stats) {
        plot->onTimeStep(storage, stats);
    }

private:
    void onPaint(wxPaintEvent& UNUSED(evt)) {
        wxPaintDC dc(this);
        wxSize canvasSize = dc.GetSize();

        // draw background
        Color backgroundColor = Color(this->GetParent()->GetBackgroundColour());
        wxBrush brush;
        brush.SetColour(wxColour(backgroundColor.darken(0.2_f)));
        dc.SetBrush(brush);
        dc.DrawRectangle(wxPoint(0, 0), canvasSize);

        this->drawLabel(dc);

        auto proxy = plot.lock();
        const Interval rangeX = proxy->rangeX();
        const Interval rangeY = proxy->rangeY();
        if (rangeX.size() <= 0._f || rangeY.size() <= 0) {
            // don't assert, it probably means there are no data to draw
            return;
        }
        this->drawAxes(dc, rangeX, rangeY);
        this->drawPlot(dc, *proxy, rangeX, rangeY);
    }

    void drawPlot(wxPaintDC& dc, IPlot& lockedPlot, const Interval rangeX, const Interval rangeY) {
        wxPen pen;
        pen.SetColour(*wxBLUE);
        dc.SetPen(pen);

        WxDrawingContext context(dc, padding, rangeX, rangeY, color);
        lockedPlot.plot(context);
    }

    void drawAxes(wxPaintDC& dc, const Interval rangeX, const Interval rangeY) {
        wxPen pen;
        pen.SetColour(*wxWHITE);
        dc.SetPen(pen);
        const wxSize size = this->GetSize();
        // find point where y-axis appears on the polot
        const Float x0 = -rangeX.lower() / rangeX.size();
        if (x0 >= 0._f && x0 <= 1._f) {
            // draw y-axis
            const Float dcX = x0 * (size.x - 2 * padding.x);
            dc.DrawLine(padding.x + dcX, size.y - padding.y, padding.x + dcX, padding.y);
        }
        // find point where x-axis appears on the plot
        const Float y0 = -rangeY.lower() / rangeY.size();
        if (y0 >= 0._f && y0 <= 1._f) {
            // draw x-axis
            const Float dcY = y0 * (size.y - 2 * padding.y);
            dc.DrawLine(padding.x, size.y - padding.y - dcY, size.x - padding.x, size.y - padding.y - dcY);
        }
    }

    void drawLabel(wxDC& dc) {
        const wxString label = name;
        wxFont font = dc.GetFont();
        font.MakeSmaller();
        dc.SetFont(font);
        const wxSize labelSize = dc.GetTextExtent(label);
        dc.DrawText(label, dc.GetSize().x - labelSize.x, 0);
    }
};

NAMESPACE_SPH_END
