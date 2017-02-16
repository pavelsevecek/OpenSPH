#pragma once

/// Simple 2D vector with integer coordinates. Provides conversion from and to wxPoint.
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mff.cuni.cz

#include "objects/Object.h"
#include <wx/gdicmn.h>

NAMESPACE_SPH_BEGIN

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


NAMESPACE_SPH_END
