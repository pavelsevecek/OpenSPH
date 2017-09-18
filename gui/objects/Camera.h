#pragma once

/// \file Camera.h
/// \brief Defines projection transforming 3D particles onto 2D screen
/// \author Pavel Sevecek (sevecek at sirrah.troja.mf.cuni.cz)
/// \date 2016-2017

#include "gui/objects/Point.h"
#include "math/AffineMatrix.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

/// Represents a particle projected onto image plane
struct ProjectedPoint {

    /// Point in image coordinates
    Point point;

    /// Projected radius of the particle
    float radius;
};

/// Ray given by origin and target point
struct Ray {
    Vector origin;
    Vector target;
};

/// \brief Interface defining a camera or view, used by a renderer.
class ICamera : public Polymorphic {
public:
    /// \brief Returns projected position of particle on the image.
    ///
    /// If the particle is outside of the image region or is clipped by the projection, returns NOTHING.
    virtual Optional<ProjectedPoint> project(const Vector& r) const = 0;

    /// \brief Returns a ray in particle coordinates corresponding to given point in the image plane.
    virtual Ray unproject(const Point point) const = 0;

    /// \brief Returns the direction of the camera.
    virtual Vector getDirection() const = 0;

    /// \param Applies zoom to the camera.
    ///
    /// This is usually equivalent to transforming the view with scaling matrix, alhough it can be implemented
    /// differently.
    /// \param magnitude Relative zoom amount, value <1 means zooming out, value >1 means zooming in.
    virtual void zoom(const float magnitude) = 0;

    /// \brief Transforms the current view by given matrix.
    ///
    /// This replaces previous transformation matrix, i.e. subsequent calls do not accumulate.
    /// \param matrix Transform matrix applied to the camera.
    virtual void transform(const AffineMatrix& matrix) = 0;

    /// \brief Moves the camera by relative offset
    virtual void pan(const Point offset) = 0;
};


struct OrthoCameraData {
    /// Field of view (zoom)
    float fov;

    /// Vectors defining camera plane
    Vector u, v;
};

class OrthoCamera : public ICamera {
private:
    Point imageSize;
    Point center;

    OrthoCameraData data;

    /// Cached transformed values
    struct {
        Vector u, v, w;
    } cached;

public:
    OrthoCamera(const Point imageSize, const Point center, OrthoCameraData data)
        : imageSize(imageSize)
        , center(center)
        , data(data) {
        cached.u = data.u;
        cached.v = data.v;
        cached.w = cross(data.u, data.v);
    }

    virtual Optional<ProjectedPoint> project(const Vector& r) const override {
        const Size x = center.x + dot(r, cached.u) * data.fov;
        const Size y = center.y + dot(r, cached.v) * data.fov;
        const Point point(x, imageSize.y - y - 1);
        return { { point, max(data.fov * float(r[H]), 1.f) } };
    }

    virtual Ray unproject(const Point point) const override {
        const float x = (point.x - center.x) / data.fov;
        const float y = ((imageSize.y - point.y - 1) - center.y) / data.fov;
        Ray ray;
        ray.origin = cached.u * x + cached.v * y;
        ray.target = ray.origin + cached.w;
        return ray;
    }

    virtual Vector getDirection() const override {
        return cached.w;
    }

    virtual void zoom(const float magnitude) override {
        ASSERT(magnitude > 0.f);
        data.fov *= magnitude;
    }

    virtual void transform(const AffineMatrix& matrix) override {
        cached.u = matrix * data.u;
        cached.v = matrix * data.v;
        cached.w = cross(cached.u, cached.v);
    }

    virtual void pan(const Point offset) override {
        center += offset;
    }
};

NAMESPACE_SPH_END
