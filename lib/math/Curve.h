#pragma once

#include "objects/containers/Array.h"
#include "objects/wrappers/Interval.h"

NAMESPACE_SPH_BEGIN

struct CurvePoint {
    float x;
    float y;

    bool operator==(const CurvePoint& other) const {
        return x == other.x && y == other.y;
    }
};

/// \brief Represents a user-defined function, defined by a set of points interpolated by either piecewise
/// linear curve or cubic spline.
class Curve {
private:
    /// Stores points together with a flag, indicating whether the following segment (i.e. segment between
    /// i-th and (i+1)th point) is cubic (if false, it is linear). The last flag has no meaning.
    Array<std::pair<CurvePoint, bool>> points;

public:
    /// \brief Creates an identity function, defined in interval [0, 1].
    Curve();

    /// \brief Creates a linear function, mapping interval \ref rangeX to interval \ref rangeY.
    Curve(const Interval& rangeX, const Interval& rangeY);

    /// \brief Creates a function from provided set of points.
    ///
    /// The points do not have to be sorted.
    Curve(Array<CurvePoint>&& curve);

    Curve(const Curve& curve);

    Curve& operator=(const Curve& curve);

    /// \brief Evaluates the function and returns the result.
    float operator()(const float x) const;

    /// \brief Returns the number of points defining the curve.
    Size getPointCnt() const;

    /// \brief Returns the position of idx-th point.
    const CurvePoint& getPoint(const Size idx) const;

    /// \brief Modifies the position of idx-th points.
    void setPoint(const Size idx, const CurvePoint& newPoint);

    /// \brief Adds a new point to the curve.
    ///
    /// The interpolation type of the newly created segment is copied from the split segment.
    void addPoint(const CurvePoint& newPoint);

    /// \brief Removes idx-th point from curve.
    void deletePoint(const Size idx);

    /// \brief Returns the interpolation type of idx-th segment.
    ///
    /// If true, the segment is interpolated using cubic spline, otherwise it uses a linear function.
    bool getSegment(const Size idx) const;

    /// \brief Modifies the interpolation type of idx-th segment.
    void setSegment(const Size idx, const bool cubic);

    /// \brief Returns the extent of the curve in x-direction.
    Interval rangeX() const;

    /// \brief Returns the extent of the curve in y-direction.
    Interval rangeY() const;

private:
    float linear(const CurvePoint& p1, const CurvePoint& p2, const float x) const;

    float cubic(const CurvePoint& p1, const CurvePoint& p2, float dy1, float dy2, float x) const;

    void sort();
};

NAMESPACE_SPH_END
