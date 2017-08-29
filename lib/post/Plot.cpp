#include "post/Plot.h"

NAMESPACE_SPH_BEGIN

/// \note SpatialPlot should be only used through derived classes, so it's ok to put templated functions here
template <typename TDerived>
void SpatialPlot<TDerived>::onTimeStep(const Storage& storage, const Statistics& UNUSED(stats)) {
    // no temporal dependence - reset everything
    points.clear();
    ranges.x = ranges.y = Interval();

    ArrayView<const Float> quantity = storage.getValue<Float>(id);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
    for (Size i = 0; i < r.size(); ++i) {
        PlotPoint p{ getX(r[i]), quantity[i] };
        points.push(p);
        ranges.x.extend(p.x);
        ranges.y.extend(p.y);
    }
    std::sort(
        points.begin(), points.end(), [](const PlotPoint& p1, const PlotPoint& p2) { return p1.x < p2.x; });
}

template <typename TDerived>
void SpatialPlot<TDerived>::clear() {
    points.clear();
    ranges.x = ranges.y = Interval();
}

template <typename TDerived>
void SpatialPlot<TDerived>::plot(IDrawingContext& dc) const {
    for (PlotPoint p : points) {
        dc.drawPoint(p);
    }
}


void TemporalPlot::onTimeStep(const Storage& storage, const Statistics& stats) {
    // add new point to the queue
    const Float t = stats.get<Float>(StatisticsId::TOTAL_TIME);
    if (t - lastTime < params.period) {
        return;
    }
    lastTime = t;

    const Float y = integral.evaluate(storage);
    points.push_back(PlotPoint{ t, y });

    // pop expired points
    bool needUpdateRange = false;
    while (this->isExpired(points.front().x, t)) {
        points.pop_front();
        needUpdateRange = true;
    }

    // update ranges
    if (needUpdateRange && params.shrinkY) {
        // we removed some points, so we don't know how much to shrink, let's construct it from scrach
        ranges.y = Interval();
        for (PlotPoint& p : points) {
            ranges.y.extend(p.y);
        }
    } else {
        // we just added points, no need to shrink the range, just extend it with the new point
        ranges.y.extend(points.back().y);
    }
    // make sure the y-range is larger than the minimal allowed value
    if (ranges.y.size() < params.minRangeY) {
        const Float dy = 0.5_f * (params.minRangeY - ranges.y.size());
        ASSERT(dy > 0._f);
        ranges.y.extend(ranges.y.upper() + dy);
        ranges.y.extend(ranges.y.lower() - dy);
    }
    if (params.fixedRangeX.empty()) {
        const Float t0 = max(points.front().x, t - params.segment);
        ranges.x = Interval(t0, t);
    } else {
        ranges.x = params.fixedRangeX;
    }
}

void TemporalPlot::clear() {
    points.clear();
    lastTime = -INFTY;
    ranges.x = ranges.y = Interval();
}

void TemporalPlot::plot(IDrawingContext& dc) const {
    if (points.empty()) {
        return;
    }
    AutoPtr<IDrawPath> path = dc.drawPath();
    for (const PlotPoint& p : points) {
        dc.drawPoint(p);
        path->addPoint(p);
    }
    path->endPath();
}

bool TemporalPlot::isExpired(const Float x, const Float t) const {
    if (params.fixedRangeX.empty()) {
        // compare with the segment
        return x < t - params.segment;
    } else {
        // compare with the range
        return !params.fixedRangeX.contains(t);
    }
}


void HistogramPlot::onTimeStep(const Storage& storage, const Statistics& UNUSED(stats)) {
    Post::HistogramParams params;
    params.id = Post::HistogramId(id);
    points = Post::getDifferentialSfd(storage, params);

    this->clear();
    for (Post::SfdPoint& p : points) {
        ranges.x.extend(p.radius);
        ranges.y.extend(p.count);
    }
}

void HistogramPlot::clear() {
    ranges.x = ranges.y = Interval();
}

void HistogramPlot::plot(IDrawingContext& dc) const {
    if (points.empty()) {
        return;
    }
    for (Size i = 0; i < points.size() - 1; ++i) {
        dc.drawLine(
            { points[i].radius, Float(points[i].count) }, { points[i + 1].radius, Float(points[i].count) });
        dc.drawLine({ points[i + 1].radius, Float(points[i].count) },
            { points[i + 1].radius, Float(points[i + 1].count) });
    }
}

NAMESPACE_SPH_END
