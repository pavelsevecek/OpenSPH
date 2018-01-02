#pragma once

/// \file Point.h
/// \brief Simple 2D vector with integer coordinates. Provides conversion from and to wxPoint.
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

#include "math/Math.h"
#include "objects/Object.h"
#include <wx/gdicmn.h>

NAMESPACE_SPH_BEGIN

/// \brief Two-dimensional point with integral coordinates
class Point {
public:
    int x, y;

    Point() = default;

    Point(const int x, const int y)
        : x(x)
        , y(y) {}

    Point(const wxPoint point)
        : x(point.x)
        , y(point.y) {}

    operator wxPoint() const {
        return wxPoint(x, y);
    }

    Point& operator+=(const Point other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    Point& operator-=(const Point other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }
};

inline float getLength(const Point p) {
    return sqrt(sqr(p.x) + sqr(p.y));
}

NAMESPACE_SPH_END
