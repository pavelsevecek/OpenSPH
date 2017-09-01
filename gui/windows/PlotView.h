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
#include <wx/frame.h>
#include <wx/panel.h>

NAMESPACE_SPH_BEGIN

struct PlotData {
    /// Plot to be drawn with associated mutex
    LockingPtr<IPlot> plot;

    /// Color of the plot
    Color color;
};

class PlotFrame;

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
        const Size defaultSelectedIdx);

private:
    void updatePlot(const Size index);

    void onRightUp(wxMouseEvent& evt);

    void onDoubleClick(wxMouseEvent& evt);

    void onMenu(wxCommandEvent& evt);

    void onPaint(wxPaintEvent& evt);

    void drawPlot(wxPaintDC& dc, IPlot& lockedPlot, const Interval rangeX, const Interval rangeY);

    void drawAxes(wxPaintDC& dc, const Interval rangeX, const Interval rangeY);

    void drawLabel(wxDC& dc);
};


class PlotFrame : public wxFrame {
private:
    LockingPtr<IPlot> plot;

public:
    PlotFrame(wxWindow* parent, const wxSize size, const wxSize padding, const LockingPtr<IPlot>& plot);
};


NAMESPACE_SPH_END
