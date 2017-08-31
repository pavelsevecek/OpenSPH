#pragma once

/// \file Plot.h
/// \brief Drawing quantity values as functions of time or spatial coordinates
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "objects/containers/List.h"
#include "objects/utility/OperatorTemplate.h"
#include "physics/Integrals.h"
#include "post/Analysis.h"
#include "quantities/Storage.h"
#include "system/Statistics.h"

NAMESPACE_SPH_BEGIN

/// \brief Point in 2D plot
struct PlotPoint : public OperatorTemplate<PlotPoint> {
    Float x, y;

    PlotPoint() = default;

    PlotPoint(const Float x, const Float y)
        : x(x)
        , y(y) {}

    PlotPoint& operator+=(const PlotPoint& other) {
        x += other.x;
        y += other.y;
        return *this;
    }
};

/// \brief Point with error bars
struct ErrorPlotPoint : public PlotPoint {
    Float dx, dy;
};


class IDrawPath : public Polymorphic {
public:
    /// Adds a next point on the path
    virtual void addPoint(const PlotPoint& point) = 0;

    /// Closes the path, connecting to the first point on the path
    virtual void closePath() = 0;

    /// Finalizes the path. Does not connect the last point to anything.
    virtual void endPath() = 0;
};

/// \brief Abstraction of a drawing context.
///
/// Object operates in plot coordinates.
class IDrawingContext : public Polymorphic {
private:
public:
    virtual void drawPoint(const PlotPoint& point) = 0;

    virtual void drawErrorPoint(const ErrorPlotPoint& point) = 0;

    /// Draws a line connecting two points. The ending points are not drawn, call \ref drawPoint manually if
    /// you wish to draw both lines and the points.
    virtual void drawLine(const PlotPoint& from, const PlotPoint& to) = 0;

    /// Draws a path connecting points.
    virtual AutoPtr<IDrawPath> drawPath() = 0;
};


/// \brief Interface for constructing generic plots from quantities stored in storage.
///
/// The plot can currently be only 2D, typically showing a quantity dependence on time or on some spatial
/// coordinate.
class IPlot : public Polymorphic {
protected:
    struct {
        Interval x;
        Interval y;
    } ranges;

public:
    /// \brief Returns the plotted range in x-coordinate.
    Interval rangeX() const {
        return ranges.x;
    }

    /// \brief Returns the plotted range in y-coordinate.
    Interval rangeY() const {
        return ranges.y;
    }

    /// \brief Returns the caption of the plot
    virtual std::string getCaption() const = 0;

    /// \brief Updates the plot with new data.
    ///
    /// Called every time step.
    virtual void onTimeStep(const Storage& storage, const Statistics& stats) = 0;

    /// \brief Clears all cached data, prepares for next run.
    virtual void clear() = 0;

    /// \brief Draws the plot into the drawing context
    virtual void plot(IDrawingContext& dc) const = 0;
};


/// \brief Base class for plots showing a dependence of given quantity on a spatial coordinate.
///
/// Currently only works with scalar quantities.
/// \todo Needs to limit the number of drawn points - we definitely dont want to draw 10^5 points.
template <typename TDerived>
class SpatialPlot : public IPlot {
protected:
    QuantityId id;
    Array<PlotPoint> points;

public:
    explicit SpatialPlot(const QuantityId id)
        : id(id) {}

    virtual std::string getCaption() const override {
        return getMetadata(id).quantityName;
    }

    virtual void onTimeStep(const Storage& storage, const Statistics& UNUSED(stats)) override;

    virtual void clear() override;

    virtual void plot(IDrawingContext& dc) const override;

private:
    Float getX(const Vector r) const {
        return static_cast<const TDerived*>(this)->getX(r);
    }
};

/// \brief Plots a dependence of given quantity on the distance from given axis.
class AxialDistributionPlot : public SpatialPlot<AxialDistributionPlot> {
private:
    Vector axis;

public:
    AxialDistributionPlot(const Vector& axis, const QuantityId id)
        : SpatialPlot<AxialDistributionPlot>(id)
        , axis(axis) {}

    INLINE Float getX(const Vector& r) const {
        return getLength(r - dot(r, axis) * axis);
    }
};

/// \brief Plots a dependence of given quantity on the distance from the origin
class SphericalDistributionPlot : public SpatialPlot<SphericalDistributionPlot> {
public:
    SphericalDistributionPlot(const QuantityId id)
        : SpatialPlot<SphericalDistributionPlot>(id) {}

    INLINE Float getX(const Vector& r) const {
        return getLength(r);
    }
};

/// \brief Plot of temporal dependence of a scalar quantity.
///
/// Plot shows a given segment of history of a quantity. This segment moves as time goes. Alternatively, the
/// segment can be (formally) infinite, meaning the plot shows the whole history of a quantity; the x-range is
/// rescaled as time goes.
class TemporalPlot : public IPlot {
public:
    /// Parameters of the plot
    struct Params {
        /// Plotted time segment
        Float segment = INFTY;

        /// Fixed x-range for the plot. If empty, a dynamic range is used.
        Interval fixedRangeX = Interval{};

        /// Minimal size of the y-range
        Float minRangeY = 0._f;

        /// When discarting points out of plotted range, shrink y-axis to fit currently visible points
        bool shrinkY = false;

        /// Time that needs to pass before a new point is added
        Float period = 0._f;
    };


private:
    /// Integral being plotted
    IntegralWrapper integral;

    /// Points on the timeline; x coordinate is time, y coordinate is the value of the quantity
    /// \todo replace with Queue
    List<PlotPoint> points;

    /// Last time a point has been added
    Float lastTime = -INFTY;

    Params params;

public:
    /// Creates a plot showing the whole history of given integral.
    TemporalPlot(const IntegralWrapper& integral, const Params& params)
        : integral(integral)
        , params(params) {
        ASSERT(params.segment > 0._f);
    }

    virtual std::string getCaption() const override {
        return integral.getName();
    }

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) override;

    virtual void clear() override;

    virtual void plot(IDrawingContext& dc) const override;

private:
    /// Checks if given point is presently expired and should be removed from the queue.
    bool isExpired(const Float x, const Float t) const;
};


/// \brief Differential histogram of quantities
///
/// Plot doesn't store any history, it is drawed each timestep independently.
class HistogramPlot : public IPlot {
private:
    /// ID of a quantity from which the histogram is constructed.
    Post::HistogramId id;

    /// Points representing the histogram
    Array<Post::SfdPoint> points;

public:
    explicit HistogramPlot(const Post::HistogramId id)
        : id(id) {}

    explicit HistogramPlot(const QuantityId id)
        : id(Post::HistogramId(id)) {}

    virtual std::string getCaption() const override {
        return getMetadata(QuantityId(id)).quantityName;
    }

    virtual void onTimeStep(const Storage& storage, const Statistics& stats) override;

    virtual void clear() override;

    virtual void plot(IDrawingContext& dc) const override;
};

NAMESPACE_SPH_END
