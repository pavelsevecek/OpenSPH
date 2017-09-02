#pragma once

/// \file GraphicsContext.h
/// \brief Implementation of IDrawingContext using wxGraphicsContext
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/objects/Color.h"
#include "post/Plot.h"
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/graphics.h>

NAMESPACE_SPH_BEGIN

class GraphicsPath : public IDrawPath {
private:
    SharedPtr<wxGraphicsContext> gc;
    wxGraphicsMatrix matrix;
    wxGraphicsPath path;
    bool first;

public:
    explicit GraphicsPath(const SharedPtr<wxGraphicsContext>& gc, const wxGraphicsMatrix& matrix)
        : gc(gc)
        , matrix(matrix) {
        path = gc->CreatePath();
        first = true;
    }

    virtual void addPoint(const PlotPoint& point) override {
        double x = point.x, y = point.y;
        matrix.TransformPoint(&x, &y);
        if (first) {
            path.MoveToPoint(x, y);
            first = false;
        } else {
            path.AddLineToPoint(x, y);
        }
    }

    virtual void closePath() override {
        path.CloseSubpath();
        gc->StrokePath(path);
    }

    virtual void endPath() override {
        gc->StrokePath(path);
    }
};

/// \brief Drawing context using wxWidgets implementation of Cairo backend
class GraphicsContext : public IDrawingContext {
private:
    SharedPtr<wxGraphicsContext> gc;

    /// Transformation matrix from plot to device coordinates
    wxGraphicsMatrix matrix;

    /// Point radius in pixels
    const Float ps = 3._f;

public:
    /// Constructs the drawing context from wxPaintDC
    GraphicsContext(wxPaintDC& dc, const Color color)
        : gc(wxGraphicsContext::Create(dc)) {
        wxPen pen;
        pen.SetColour(wxColour(color));
        gc->SetPen(pen);
        matrix = gc->CreateMatrix();
    }

    /// Constructs the drawing context from wxMemoryDC
    GraphicsContext(wxMemoryDC& dc, const Color color)
        : gc(wxGraphicsContext::Create(dc)) {
        wxPen pen;
        pen.SetColour(wxColour(color));
        gc->SetPen(pen);
        matrix = gc->CreateMatrix();
    }

    virtual void drawPoint(const PlotPoint& point) override {
        double x = point.x, y = point.y;
        matrix.TransformPoint(&x, &y);
        gc->DrawEllipse(x - ps / 2, y - ps / 2, ps, ps);
    }

    virtual void drawErrorPoint(const ErrorPlotPoint& point) override {
        this->drawPoint(point);
    }

    virtual void drawLine(const PlotPoint& from, const PlotPoint& to) override {
        double x1 = from.x, y1 = from.y;
        matrix.TransformPoint(&x1, &y1);
        double x2 = to.x, y2 = to.y;
        matrix.TransformPoint(&x2, &y2);
        gc->StrokeLine(x1, y1, x2, y2);
    }

    virtual AutoPtr<IDrawPath> drawPath() override {
        return makeAuto<GraphicsPath>(gc, matrix);
    }

    virtual void setTransformMatrix(const AffineMatrix2& m) override {
        matrix.Set(m(0, 0), m(0, 1), m(1, 0), m(1, 1), m(0, 2), m(1, 2));
    }
};


/// \brief Defines a continuous function from a set of discrete poitns
/*class IInterpolation : public Polymorphic {
protected:
    Array<PlotPoint> points;

public:
    IInterpolation(Array<PlotPoint>&& points)
        : points(std::move(points)) {
        auto compare = [](PlotPoint& p1, PlotPoint& p2) { return p1.x < p2.x; };
        std::sort(this->points.begin(), this->points.end(), compare);
    }

    virtual Array<PlotPoint> interpolate(const Interval& range, const Float step) const = 0;
};


class LinearInterpolation : public IInterpolation {
public:
    LinearInterpolation(Array<PlotPoint>&& points)
        : IInterpolation(std::move(points)) {}

    virtual Array<PlotPoint> interpolate(const Interval& range, const Float step) const {
        Array<PlotPoint> result;
        Size index = 0;
        const Float x0 = points[index].x;
        if (range.lower() < x0) {
            // extend using tangent of two left-most points
        }
        for (Float x = range.lower(); x < range.upper(); x += step) {

            //            if ()
        }
        // find index
        //      const Size i0;
        return result;
    }
};*/

/*class CatmullRomInterpolation : public IInterpolation {
public:
    Float operator()() const {
        PlotPoint p0, p1, p2, p3;
        // compute curve parameters, using centripetal Catmull-Rom parametrization
        const Float t1 = getParam(p0, p1);
        const Float t2 = getParam(p1, p2) + t1;
        const Float t3 = getParam(p2, p3) + t2;
        return t3;
    }

private:
    INLINE Float getParam(const PlotPoint p1, const PlotPoint p2) const {
        return root<4>(sqr(p2.x - p1.x) + sqr(p2.y - p1.y));
    }
};*/


NAMESPACE_SPH_END
