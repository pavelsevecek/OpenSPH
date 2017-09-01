#pragma once

/// \file SvgContext.h
/// \brief Implementation of IDrawingContext for creating vector images (.svg)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/objects/Point.h"
#include "post/Plot.h"
#include <wx/dcsvg.h>

NAMESPACE_SPH_BEGIN

class SvgPath : public IDrawPath {
private:
    wxSVGFileDC& dc;
    Size pointSize;
    PlotPoint prev{ -1, -1 };
    PlotPoint first;

public:
    SvgPath(wxSVGFileDC& dc, const Size pointSize)
        : dc(dc)
        , pointSize(pointSize) {}

    virtual void addPoint(const PlotPoint& point) override {
        if (prev == PlotPoint{ -1, -1 }) {
            first = point;
        } else {
            dc.DrawLine({ prev.x, prev.y }, { point.x, point.y });
        }
        prev = point;
    }

    virtual void closePath() override {
        dc.DrawLine({ prev.x, prev.y }, { first.x, first.y });
    }

    virtual void endPath() override {
        // do nothing
    }
};

class SvgContext : public IDrawingContext {
private:
    wxSVGFileDC dc;
    Size pointSize = 3;

public:
    SvgContext(const Path& path, const Point size, const double dpi = 72)
        : dc(path.native(), size.x, size.y, dpi) {}

    virtual void drawPoint(const PlotPoint& point) override {
        dc.DrawCircle(point.x, point.y, pointSize);
    }

    virtual void drawErrorPoint(const ErrorPlotPoint& point) override {
        dc.DrawCircle(point.x, point.y, pointSize);
    }

    virtual void drawLine(const PlotPoint& from, const PlotPoint& to) override {
        dc.DrawLine({ from.x, from.y }, { to.x, to.y });
    }

    virtual AutoPtr<IDrawPath> drawPath() override {
        return makeAuto<SvgPath>(dc, pointSize);
    }
};

NAMESPACE_SPH_END
