#pragma once

/// \file Plot.h
/// \brief Drawing quantity values as functions of time or spatial coordinates
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/OperatorTemplate.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// \brief Point in 2D plot
struct PlotPoint : public OperatorTemplate<PlotPoint> {
    Float x, y;

    PlotPoint& operator+=(const PlotPoint& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
};

/// \brief Point with error bars
struct ErrorPlotPlot : public OperatorTemplate<ErrorPlotPlot> {
    Vector point;
};

namespace Abstract {
    /// \brief Defines a continuous function from a set of discrete poitns
    class Interpolation : public Polymorphic {
    protected:
        Array<PlotPoint> points;

    public:
        Interpolation(Array<PlotPoint>&& points)
            : points(std::move(points)) {
            auto compare = [](PlotPoint& p1, PlotPoint& p2) { return p1.x < p2.x; };
            std::sort(this->points.begin(), this->points.end(), compare);
        }

        virtual Array<PlotPoint> interpolate(const Range& range, const Float step) const = 0;
    };
}

class LinearInterpolation : public Abstract::Interpolation {
public:
    LinearInterpolation(Array<PlotPoint>&& points)
        : Abstract::Interpolation(std::move(points)) {}

    virtual Array<PlotPoint> interpolate(const Range& range, const Float step) const {
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
};

class CatmullRomInterpolation : public Abstract::Interpolation {
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
};

namespace Abstract {

    /// \brief Interface for constructing generic plots from quantities stored in storage.
    ///
    /// The plot can currently be only 2D, typically showing a quantity dependence on time or on some spatial
    /// coordinate.
    class Plot : public Polymorphic {
    protected:
        struct {
            Range x;
            Range y;
        } ranges;

    public:
        Range getRangeX() const {
            return ranges.x;
        }

        Range getRangeY() const {
            return ranges.y;
        }

        virtual void onTimeStep(const Storage& storage, const Statistics& stats) = 0;

        virtual Float plot(const Float x) const = 0;
    };
}

/// \brief Plot of a simple predefined function.
///
/// Can be used for comparison between computed numerical results and an analytical solution.
/*class FunctionPlot : public Abstract::Plot {
private:
    std::function<Float(Float)> func;
    Size numPoints;

public:
    FunctionPlot(const std::function<Float(Float)>& func, const Range rangeX, const Size numPoints)
        : func(func)
        , numPoints(numPoints) {
        ASSERT(func != nullptr);
        ranges.x = rangeX;
        Array<PlotPoint> points = this->plot();
        for (PlotPoint p : points) {
            ranges.y.extend(p.y);
        }
    }

    virtual void onTimeStep(const Storage& UNUSED(storage), const Statistics& UNUSED(stats)) override {}

    virtual Array<PlotPoint> plot() const override {
        Array<PlotPoint> points;
        for (Float x = ranges.x.lower(); x < ranges.x.upper(); x += ranges.x.size() / numPoints) {
            points.push(PlotPoint{ x, func(x) });
        }
        return points;
    }
};*/

/// Plot averaging quantities
/*class WeightedPlot : public Abstract::Plot {
protected:
    Array<Float> values;
    Array<Float> weights;

public:
    virtual void addPoint(const Float x, const Float y, const Float w) {
        if (!range.contains(x)) {
            return;
        }
        const Size idx = (x - range.lower()) / range.size() * (values.size() - 1);
        values[idx] = (values[idx] * weights[idx] + y * w) / (weights[idx] + w);
        weights[idx] += w;
    }

    template <typename TFunctor>
    void plot(TFunctor&& functor) {
        for (Size i = 0; i < values.size(); ++i) {
            const Float x = range.lower() + (range.size() * i) / (values.size() - 1);
            functor(x, values[i]);
        }
    }

    /// Compares two plots of the same type
    bool equals(const Float UNUSED(tolerance)) const {
        return true;
    }
};*/

/*template <typename TDerived>
class SpatialPlot : public Abstract::Plot {
protected:
    QuantityId id;
    Array<PlotPoint> points;

public:
    SpatialPlot(const QuantityId id)
        : id(id) {}

    virtual void onTimeStep(const Storage& storage, const Statistics& UNUSED(stats)) override {
        ArrayView<const Float> quantity = storage.getValue<Float>(id);
        ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITIONS);
        Array<PlotPoint> points;
        points.reserve(r.size());
        for (Size i = 0; i < r.size(); ++i) {
            points.push(PlotPoint{ static_cast<const TDerived*>(this)->func(r[i]), quantity[i] });
        }
        std::sort(points.begin(), points.end(), [](const PlotPoint& p1, const PlotPoint& p2) {
            return p1.x < p2.x;
        });
    }

    virtual Array<PlotPoint> plot() const override {
        return points.clone();
    }
};

class AxialDistributionPlot : public SpatialPlot<AxialDistributionPlot> {
private:
    Vector axis;

public:
    AxialDistributionPlot(const Vector& axis, const QuantityId id)
        : SpatialPlot<AxialDistributionPlot>(id)
        , axis(axis) {}

    INLINE Float func(const Vector& r) const {
        return getLength(r - dot(r, axis) * axis);
    }
};


class SphericalDistributionPlot : public SpatialPlot<SphericalDistributionPlot> {
public:
    SphericalDistributionPlot(const QuantityId id)
        : SpatialPlot<SphericalDistributionPlot>(id) {}

    INLINE Float func(const Vector& r) const {
        return getLength(r);
    }
};

/// \brief Plot of temporal dependence of a scalar quantity.
///
/// Plot shows a given segment of history of a quantity. This segment moves as time goes. Alternatively, the
/// segment can be (formally) infinite, meaning the plot shows the whole history of a quantity; the x-range is
/// rescaled as time goes.
class TemporalPlot : public Abstract::Plot {
private:
    Range timeRange;

public:
    /// Creates a plot showing the whole history of a quantity.
    TemporalPlot() = default;

    /// Creates a plot given the time segment to show.
    TemporalPlot(const Range& timeRange)
        : timeRange(timeRange) {}
};

*/
NAMESPACE_SPH_END
