#pragma once

/// \file SvgContext.h
/// \brief Implementation of IDrawingContext for creating vector images (.svg)
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "gui/objects/Point.h"
#include "io/Path.h"
#include "post/Plot.h"
#include <wx/dcsvg.h>

NAMESPACE_SPH_BEGIN

class SvgPath : public IDrawPath {
private:
    wxSVGFileDC& dc;
    AffineMatrix2 matrix;

    PlotPoint prev{ -1, -1 };
    PlotPoint first;

public:
    SvgPath(wxSVGFileDC& dc, const AffineMatrix2& matrix)
        : dc(dc)
        , matrix(matrix) {}

    virtual void addPoint(const PlotPoint& point) override {
        if (prev == PlotPoint{ -1, -1 }) {
            first = point;
        } else {
            const PlotPoint p1 = matrix.transformPoint(prev);
            const PlotPoint p2 = matrix.transformPoint(point);
            dc.DrawLine({ int(p1.x), int(p1.y) }, { int(p2.x), int(p2.y) });
        }
        prev = point;
    }

    virtual void closePath() override {
        const PlotPoint p1 = matrix.transformPoint(prev);
        const PlotPoint p2 = matrix.transformPoint(first);
        dc.DrawLine({ int(p1.x), int(p1.y) }, { int(p2.x), int(p2.y) });
    }

    virtual void endPath() override {
        // do nothing
    }
};

class SvgContext : public IDrawingContext {
private:
    wxSVGFileDC dc;
    int pointSize = 3;
    AffineMatrix2 matrix;

public:
    SvgContext(const Path& path, const Point size, const double dpi = 72)
        : dc(path.native(), size.x, size.y, dpi) {}

    virtual void drawPoint(const PlotPoint& point) override {
        const PlotPoint p = matrix.transformPoint(point);
        dc.DrawCircle(int(p.x), int(p.y), pointSize);
    }

    virtual void drawErrorPoint(const ErrorPlotPoint& point) override {
        const PlotPoint p = matrix.transformPoint(point);
        dc.DrawCircle(int(p.x), int(p.y), pointSize);
    }

    virtual void drawLine(const PlotPoint& from, const PlotPoint& to) override {
        const PlotPoint p1 = matrix.transformPoint(from);
        const PlotPoint p2 = matrix.transformPoint(to);
        dc.DrawLine({ int(p1.x), int(p1.y) }, { int(p2.x), int(p2.y) });
    }

    virtual AutoPtr<IDrawPath> drawPath() override {
        return makeAuto<SvgPath>(dc, matrix);
    }

    virtual void setTransformMatrix(const AffineMatrix2& newMatrix) override {
        matrix = newMatrix;
    }
};

NAMESPACE_SPH_END
