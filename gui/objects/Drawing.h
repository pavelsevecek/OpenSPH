#pragma once

#include "gui/objects/Color.h"
#include "post/Plot.h"
#include <wx/dc.h>
#include <wx/graphics.h>

NAMESPACE_SPH_BEGIN

class WxDrawPath : public IDrawPath {
private:
    SharedPtr<wxGraphicsContext> gc;
    wxGraphicsMatrix matrix;
    wxGraphicsPath path;
    bool first;

public:
    explicit WxDrawPath(const SharedPtr<wxGraphicsContext>& gc, const wxGraphicsMatrix& matrix)
        : gc(gc)
        , matrix(matrix) {
        path = gc->CreatePath();
        first = true;
    }

    virtual void addPoint(const PlotPoint& point) override {
        PlotPoint q = point;
        matrix.TransformPoint(&q.x, &q.y);
        if (first) {
            path.MoveToPoint(q.x, q.y);
            first = false;
        } else {
            path.AddLineToPoint(q.x, q.y);
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

class WxDrawingContext : public IDrawingContext {
private:
    SharedPtr<wxGraphicsContext> gc;

    /// Transformation matrix from plot to device coordinates
    wxGraphicsMatrix matrix;

    /// Point radius in pixels
    const Float ps = 3._f;

public:
    explicit WxDrawingContext(wxPaintDC& dc,
        const wxSize padding,
        const Interval rangeX,
        const Interval rangeY,
        const Color color)
        : gc(wxGraphicsContext::Create(dc)) {
        ASSERT(rangeX.size() > 0 && rangeY.size() > 0);
        const wxSize size = dc.GetSize() - 2 * padding;
        const Float scaleX = size.x / rangeX.size();
        const Float scaleY = -size.y / rangeY.size();
        const Float transX = padding.x - scaleX * rangeX.lower();
        const Float transY = size.y + padding.y - scaleY * rangeY.lower();
        matrix = gc->CreateMatrix(scaleX, 0.f, 0.f, scaleY, transX, transY);

        wxPen pen;
        pen.SetColour(wxColour(color));
        gc->SetPen(pen);
    }

    virtual void drawPoint(const PlotPoint& point) override {
        PlotPoint q = point;
        matrix.TransformPoint(&q.x, &q.y);
        gc->DrawEllipse(q.x - ps / 2, q.y - ps / 2, ps, ps);
    }

    virtual void drawErrorPoint(const ErrorPlotPoint& point) override {
        this->drawPoint(point);
    }

    virtual void drawLine(const PlotPoint& from, const PlotPoint& to) override {
        PlotPoint q1 = from;
        matrix.TransformPoint(&q1.x, &q1.y);
        PlotPoint q2 = to;
        matrix.TransformPoint(&q2.x, &q2.y);
        gc->StrokeLine(q1.x, q1.y, q2.x, q2.y);
    }

    virtual AutoPtr<IDrawPath> drawPath() override {
        return makeAuto<WxDrawPath>(gc, matrix);
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
