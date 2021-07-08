#pragma once

/// \file PlotView.h
/// \brief Drawing of plots
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2021

#include "gui/objects/GraphicsContext.h"
#include "gui/objects/Plots.h"
#include "gui/objects/Point.h"
#include "physics/Integrals.h"
#include "thread/CheckFunction.h"
#include <wx/frame.h>
#include <wx/panel.h>

class wxBoxSizer;

NAMESPACE_SPH_BEGIN

class PlotPage;

struct TicsParams {
    Size minCnt = 6;
    Size digits = 3;
};

class PlotView : public wxPanel {
private:
    wxSize padding;
    SharedPtr<Array<PlotData>> list;

    struct {
        LockingPtr<IPlot> plot;
        Rgba color;
    } cached;

    Optional<TicsParams> ticsParams;

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
        Optional<TicsParams> ticsParams);

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


class PlotPage : public wxPanel {
private:
    LockingPtr<IPlot> plot;
    wxSize padding;

    PlotView* plotView;

public:
    PlotPage(wxWindow* parent, const wxSize size, const wxSize padding, const LockingPtr<IPlot>& plot);

private:
    wxBoxSizer* createToolbar(const Size toolbarHeight);

    void saveImage(const Path& path);

    void saveData(const Path& path);
};


NAMESPACE_SPH_END
