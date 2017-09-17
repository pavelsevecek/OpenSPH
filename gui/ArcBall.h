#pragma once

/// \file ArcBall.h
/// \brief Helper class for rotating objects by mouse drag
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2017

#include "gui/objects/Point.h"
#include "math/Quat.h"

NAMESPACE_SPH_BEGIN

/// \brief Helper object providing rotation matrix based on mouse drag.
///
/// Member function correspond need to be called from mouse events, function \ref drag then returns the
/// rotation matrix. The functions must be called in order: click - drag - drag - ... - drag - stop - click -
/// drag ..., etc. Order is checked by assert.
///
/// Done according to https://gist.github.com/vilmosioo/5318327
class ArcBall {
private:
    /// Starting point of the rotation
    Vector start{ NAN };

    /// Size of the image
    Point size;

public:
    ArcBall() {
        size = Point(0, 0);
    }

    explicit ArcBall(const Point size)
        : size(size) {}

    void resize(const Point newSize) {
        size = newSize;
    }

    /// \brief Called on mouse click, starting the rotation
    void click(const Point point) {
        start = this->mapToSphere(point);
    }

    /// \brief Called when mouse moves, rotating the object
    ///
    /// \return New rotation matrix of the object
    AffineMatrix drag(const Point point) {
        ASSERT(isReal(start));
        const Vector end = this->mapToSphere(point);
        const Vector perp = cross(start, end);
        if (getSqrLength(perp) > EPS) {
            Quat q;
            q[0] = perp[0];
            q[1] = perp[1];
            q[2] = perp[2];
            q[3] = dot(start, end);
            return q.convert();
        } else {
            return AffineMatrix::identity();
        }
    }

private:
    Vector mapToSphere(const Point point) {
        // rescale to <-1, 1> and invert y
        ASSERT(size.x > 0 && size.y > 0);
        const Vector p(2.f * float(point.x) / size.x - 1.f, 1.f - 2.f * float(point.y) / size.y, 0._f);

        const Float lengthSqr = getSqrLength(p);
        if (lengthSqr > 1.f) {
            const Float length = sqrt(lengthSqr);
            return p / length;
        } else {
            return Vector(p[X], p[Y], sqrt(1.f - lengthSqr));
        }
    }
};

NAMESPACE_SPH_END
