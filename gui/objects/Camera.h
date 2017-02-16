#pragma once

/// Defines projection transforming 3D particles onto 2D screen
/// Pavel Sevecek 2017
/// sevecek at sirrah.troja.mf.cuni.cz

#include "geometry/Vector.h"
#include "gui/objects/Point.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

namespace Abstract {
    class Camera : public Polymorphic {
    public:
        /// Returns projected position of particle on the image. If the particle is outside of the image
        /// region or is clipped by the projection, returns NOTHING.
        virtual Optional<Point> project(const Vector& r) const = 0;

        /// Returns projected size at given position in 3D space. This is basically derivative of transform
        /// function.
        virtual Optional<float> projectedSize(const Vector& r, const float size) const = 0;

        /// Zoom the camera.
        /// \param magnitude Relative amount of zooming. Value <1 means zooming out, value >1 means zooming
        /// in.
        virtual void zoom(const float magnitude) = 0;

        /// Moves the camera by relative offset
        virtual void pan(const Point offset) = 0;
    };
}


struct OrthoCameraData {
    float fov;
    float cutoff;
    Vector u;
    Vector v;
};

class OrthoCamera : public Abstract::Camera {
private:
    Point imageSize;
    Point center;
    float fov;
    float cutoff;
    Vector u, v, w;

public:
    OrthoCamera(const Point imageSize, const Point center, OrthoCameraData data)
        : imageSize(imageSize)
        , center(center)
        , fov(data.fov)
        , cutoff(data.cutoff)
        , u(data.u)
        , v(data.v) {
        w = cross(u, v);
    }

    virtual Optional<Point> project(const Vector& r) const override {
        if (abs(dot(w, r)) < cutoff) {
            return Point(center.x + dot(r, u) * fov, imageSize.y - (center.y + dot(r, v) * fov) - 1);
        } else {
            return NOTHING;
        }
    }

    virtual Optional<float> projectedSize(const Vector& r, const float size) const override {
        if (abs(dot(w, r)) < cutoff) {
            return max(fov * size, 1.f);
        } else {
            return NOTHING;
        }
    }

    virtual void zoom(const float magnitude) override {
        ASSERT(magnitude > 0.f);
        fov *= magnitude;
    }

    virtual void pan(const Point offset) override {
        center += offset;
    }
};

NAMESPACE_SPH_END
