#pragma once

/// \file PlotView.h
/// \brief Drawing of plots
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/objects/GraphicsContext.h"
#include "gui/objects/Point.h"
#include "objects/wrappers/LockingPtr.h"
#include "physics/Integrals.h"
#include "post/Plot.h"
#include "thread/CheckFunction.h"
#include <wx/frame.h>
#include <wx/panel.h>

class wxBoxSizer;

NAMESPACE_SPH_BEGIN

struct PlotData {
    /// Plot to be drawn with associated mutex
    LockingPtr<IPlot> plot;

    /// Color of the plot
    Rgba color;
};

class PlotFrame;

class PlotView : public wxPanel {
private:
    wxSize padding;
    SharedPtr<Array<PlotData>> list;

    struct {
        LockingPtr<IPlot> plot;
        Rgba color;
    } cached;

    bool showLabels;

public:
    /// Include zero in x-range
    bool addZeroX = false;

    /// Include zero in y-range
    bool addZeroY = false;


    PlotView(wxWindow* parent,
        const wxSize size,
        const wxSize padding,
        const SharedPtr<Array<PlotData>>& list,
        const Size defaultSelectedIdx,
        const bool showLabels);

    void resize(const Pixel size);

    /// \brief Returns the transformation matrix for managed plot.
    AffineMatrix2 getPlotTransformMatrix(const Interval& rangeX, const Interval& rangeY) const;

    void drawAxes(wxDC& dc, const Interval rangeX, const Interval rangeY);

private:
    void updatePlot(const Size index);

    /// Wx handlers

    void onRightUp(wxMouseEvent& evt);

    void onDoubleClick(wxMouseEvent& evt);

    void onMenu(wxCommandEvent& evt);

    void onPaint(wxPaintEvent& evt);

    /// Helper drawing functions

    void drawPlot(wxPaintDC& dc, IPlot& lockedPlot, const Interval rangeX, const Interval rangeY);

    void drawCaption(wxDC& dc, IPlot& lockedPlot);
};


class PlotFrame : public wxFrame {
private:
    LockingPtr<IPlot> plot;
    wxSize padding;

    PlotView* plotView;

public:
    PlotFrame(wxWindow* parent, const wxSize size, const wxSize padding, const LockingPtr<IPlot>& plot);

private:
    wxBoxSizer* createToolbar(const Size toolbarHeight);

    void saveImage(const Path& path);

    void saveData(const Path& path);
};


NAMESPACE_SPH_END
