#pragma once

/// \file ArcBall.h
/// \brief Helper class for rotating objects by mouse drag
/// \author Pavel Sevecek (sevecek at sirrah.troja.mff.cuni.cz)
/// \date 2016-2018

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
    Pixel size;

public:
    ArcBall() {
        size = Pixel(0, 0);
    }

    explicit ArcBall(const Pixel size)
        : size(size) {}

    void resize(const Pixel newSize) {
        size = newSize;
    }

    /// \brief Called on mouse click, starting the rotation
    void click(const Pixel point, const Pixel pivot) {
        start = this->mapToSphere(point, pivot);
    }

    /// \brief Called when mouse moves, rotating the object.
    ///
    /// \param point Current mouse position in image space.
    /// \param pivot Center of rotation in image space.
    /// \return New rotation matrix of the object
    AffineMatrix drag(const Pixel point, const Pixel pivot) {
        ASSERT(isReal(start));
        const Vector end = this->mapToSphere(point, pivot);
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
    Vector mapToSphere(const Pixel point, const Pixel pivot) {
        // rescale to <-1, 1> and invert y
        ASSERT(size.x > 0 && size.y > 0);
        /*const Vector p(
            2.f * float(point.x - pivot.x) / size.x, 2.f * float(pivot.y - point.y) / size.y, 0._f);

        const Float lengthSqr = getSqrLength(p);
        if (lengthSqr > 1.f) {
            const Float length = sqrt(lengthSqr);
            return p / length;
        } else {
            return Vector(p[X], p[Y], sqrt(1.f - lengthSqr));
        }*/
        const Float phi = 2._f * PI * Float(point.x - pivot.x) / size.x;
        const Float theta = PI * Float(pivot.y - point.y) / size.y;

        /*const Float s = sin(theta);
        const Float c = cos(theta);*/
        (void)theta;
        return Vector(0.f, sin(phi), 0.f);
    }
};

NAMESPACE_SPH_END
