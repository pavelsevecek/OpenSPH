#pragma once

/// \file PlotView.h
/// \brief Drawing of plots
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/objects/GraphicsContext.h"
#include "gui/objects/Point.h"
#include "objects/wrappers/LockingPtr.h"
#include "physics/Integrals.h"
#include "post/Plot.h"
#include "thread/CheckFunction.h"
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

struct PlotData {
    /// Plot to be drawn with associated mutex
    LockingPtr<IPlot> plot;

    /// Color of the plot
    Color color;
};

class PlotView : public wxPanel {
private:
    wxSize padding;
    SharedPtr<Array<PlotData>> list;

    struct {
        LockingPtr<IPlot> plot;
        Color color;
        std::string name;
    } cached;

public:
    PlotView(wxWindow* parent,
        const wxSize size,
        const wxSize padding,
        const SharedPtr<Array<PlotData>>& list,
        const Size defaultSelectedIdx)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, size)
        , padding(padding)
        , list(list) {
        this->SetMaxSize(size);
        Connect(wxEVT_PAINT, wxPaintEventHandler(PlotView::onPaint));
        Connect(wxEVT_RIGHT_UP, wxMouseEventHandler(PlotView::onRightUp));

        this->updatePlot(defaultSelectedIdx);
    }

private:
    void updatePlot(const Size index) {
        PlotData& data = (*list)[index];
        cached.name = data.plot->getCaption();
        cached.color = data.color;

        // plot needs to be synchronized as it is updated from different thread, hopefully neither updating
        // nor drawing will take a lot of time, so we can simply lock the pointer.
        cached.plot = data.plot;
    }

    void onRightUp(wxMouseEvent& UNUSED(evt)) {
        wxMenu menu;
        Size index = 0;
        for (PlotData& data : *list) {
            menu.Append(index++, data.plot->getCaption());
        }

        menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(PlotView::onMenu), nullptr, this);
        this->PopupMenu(&menu);
    }

    void onMenu(wxCommandEvent& evt) {
        CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
        const Size index = evt.GetId();
        ASSERT(index < list->size());
        this->updatePlot(index);
        this->Refresh();
    }

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

        auto proxy = cached.plot.lock();
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
        GraphicsContext context(dc, padding, rangeX, rangeY, cached.color);
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
        const wxString label = cached.name;
        wxFont font = dc.GetFont();
        font.MakeSmaller();
        dc.SetFont(font);
        const wxSize labelSize = dc.GetTextExtent(label);
        dc.DrawText(label, dc.GetSize().x - labelSize.x, 0);
    }
};

NAMESPACE_SPH_END
