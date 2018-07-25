#include "post/Plot.h"
#include "io/Logger.h"
#include "post/Point.h"
#include "quantities/Storage.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

/// \note SpatialPlot should be only used through derived classes, so it's ok to put templated functions here
template <typename TDerived>
void SpatialPlot<TDerived>::onTimeStep(const Storage& storage, const Statistics& UNUSED(stats)) {
    // no temporal dependence - reset everything
    this->clear();

    Array<PlotPoint> particlePoints;
    ArrayView<const Float> quantity = storage.getValue<Float>(id);
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        PlotPoint p{ getX(r[i]), quantity[i] };
        particlePoints.push(p);
        ranges.x.extend(p.x);
        ranges.y.extend(p.y);
    }
    std::sort(particlePoints.begin(), particlePoints.end(), [](PlotPoint& p1, PlotPoint& p2) {
        return p1.x < p2.x;
    });

    if (!binCnt) {
        points = std::move(particlePoints);
    } else {
        ASSERT(binCnt.value() >= 1);
        points.resize(binCnt.value());
        Array<Size> weights(binCnt.value());
        points.fill(PlotPoint(0.f, 0.f));
        weights.fill(0);

        const Float lastX = particlePoints[particlePoints.size() - 1].x;
        for (PlotPoint& p : particlePoints) {
            const Size bin = min(Size(p.x * (binCnt.value() - 1) / lastX), binCnt.value() - 1);
            points[bin] += p;
            weights[bin]++;
        }

        for (Size i = 0; i < points.size(); ++i) {
            if (weights[i] > 0) {
                points[i].x /= weights[i];
                points[i].y /= weights[i];
            } else {
                ASSERT(points[i] == PlotPoint(0.f, 0.f));
            }
        }
    }
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

RadialDistributionPlot::RadialDistributionPlot(const QuantityId id, const Optional<Size> binCnt)
    : SpatialPlot<RadialDistributionPlot>(id, binCnt) {}


void TemporalPlot::onTimeStep(const Storage& storage, const Statistics& stats) {
    // add new point to the queue
    const Float t = stats.get<Float>(StatisticsId::RUN_TIME);
    if (t - lastTime < actPeriod) {
        return;
    }
    lastTime = t;

    const Float y = integral.evaluate(storage);
    points.pushBack(PlotPoint{ t, y });

    if (params.segment == INFTY && points.size() > params.maxPointCnt) {
        // plot is unnecessarily detailed, drop every second point to reduce the drawing time
        Queue<PlotPoint> newPoints;
        for (Size i = 0; i < points.size(); i += 2) {
            newPoints.pushBack(points[i]);
        }
        points = std::move(newPoints);
        // also add new points with double period
        actPeriod *= 2._f;
    }

    // pop expired points
    bool needUpdateRange = false;
    while (!points.empty() && this->isExpired(points.front().x, t)) {
        points.popFront();
        needUpdateRange = true;
    }

    // update ranges
    if (needUpdateRange && params.shrinkY) {
        // we removed some points, so we don't know how much to shrink, let's construct it from scrach
        ranges.y = Interval();
        for (PlotPoint& p : points) {
            ranges.y.extend(p.y);
        }
    } else if (!points.empty()) {
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
    if (points.empty()) {
        ranges.x = Interval(); // nothing to draw
    } else if (params.fixedRangeX.empty()) {
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
    actPeriod = params.period;
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
    params.binCnt = 20;
    if (interval) {
        params.range = interval.value();
    }
    points = Post::getDifferentialHistogram(storage, id, Post::HistogramSource::PARTICLES, params);

    this->clear();
    for (Post::HistPoint& p : points) {
        ranges.x.extend(p.value);
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
            { points[i].value, Float(points[i].count) }, { points[i + 1].value, Float(points[i].count) });
        dc.drawLine({ points[i + 1].value, Float(points[i].count) },
            { points[i + 1].value, Float(points[i + 1].count) });
    }
}

void SfdPlot::onTimeStep(const Storage& storage, const Statistics& UNUSED(stats)) {
    Post::HistogramParams params;
    points = Post::getCummulativeHistogram(
        storage, Post::HistogramId::EQUIVALENT_MASS_RADII, Post::HistogramSource::PARTICLES, params);

    this->clear();
    sfd.clear();
    sfd.reserve(points.size());
    for (Post::HistPoint& p : points) {
        ASSERT(p.value > 0._f && p.count > 0);
        const Float value = log10(p.value);
        const Float count = log10(Float(p.count));
        ranges.x.extend(value);
        ranges.y.extend(count);
        sfd.emplaceBack(PlotPoint{ value, count });
    }
    points = Array<Post::HistPoint>();
}

void SfdPlot::plot(IDrawingContext& dc) const {
    if (sfd.empty()) {
        return;
    }
    AutoPtr<IDrawPath> path = dc.drawPath();
    for (Size i = 0; i < sfd.size() - 1; ++i) {
        dc.drawPoint(sfd[i]);
        path->addPoint(sfd[i]);
    }
    dc.drawPoint(sfd.back());
    path->addPoint(sfd.back());
    path->endPath();
}

Array<Float> getLinearTics(const Interval& interval, const Size minCount) {
    Float order = floor(log10(interval.size()));

    auto getTicsInterval = [interval](const Float step) {
        return Interval(ceil(interval.lower() / step) * step, floor(interval.upper() / step) * step);
    };

    Float step;
    while (true) {
        step = pow(10._f, order);
        ASSERT(step >= std::numeric_limits<Float>::denorm_min());
        if (getTicsInterval(step).size() < step * minCount) {
            order--;
        } else {
            break;
        }
    }

    // Now we have step 10^order, which might be too small, we thus also allow step 2*10^order (2, 4, 6, ...)
    // and 5*10^order (5, 10, 15, ...). This can be modified if necessary.
    if (getTicsInterval(5 * step).size() >= 5 * step * minCount) {
        step *= 5;
    } else if (getTicsInterval(2 * step).size() >= 2 * step * minCount) {
        step *= 2;
    }
    const Interval ticsInterval = getTicsInterval(step);

    Array<Float> tics;
    for (Float x = ticsInterval.lower(); x <= ticsInterval.upper() + EPS * step; x += step) {
        tics.push(x);
    }
    ASSERT(tics.size() >= minCount && tics.size() < 10 * minCount);
    return tics;
}

Array<Float> getLogTics(const Interval& interval, const Size minCount) {
    ASSERT(interval.lower() > 0._f);
    const Float fromOrder = floor(log10(interval.lower()));
    const Float toOrder = floor(log10(interval.upper()));
    ASSERT(toOrder >= fromOrder);
    Float stepOrder = 1; // try stepping in integer orders (1, 10, 100, ...)

    while (toOrder - fromOrder < minCount * stepOrder) {
        stepOrder /= 2._f;
    }

    Array<Float> tics;
    for (Float order = fromOrder + 1; order <= toOrder; order += stepOrder) {
        tics.push(pow(10._f, order));
    }
    ASSERT(tics.size() >= minCount && tics.size() < 10 * minCount);
    return tics;
}

NAMESPACE_SPH_END
