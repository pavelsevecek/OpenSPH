#pragma once

/// \file Camera.h
/// \brief Defines projection transforming 3D particles onto 2D screen
/// \author Pavel Sevecek (sevecek at sirrah.troja.mf.cuni.cz)
/// \date 2016-2018

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
struct CameraRay {
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
    virtual CameraRay unproject(const Point point) const = 0;

    /// \brief Returns the direction of the camera.
    virtual Vector getDirection() const = 0;

    /// \param Applies zoom to the camera.
    ///
    /// This is usually equivalent to transforming the view with scaling matrix, alhough it can be implemented
    /// differently.
    /// \param fixedPoint Point that remains fixed after the zoom (for magnitude != 1, there is exactly one)
    /// \param magnitude Relative zoom amount, value <1 means zooming out, value >1 means zooming in.
    virtual void zoom(const Point fixedPoint, const float magnitude) = 0;

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

    /// Z-offset of the camera
    float zoffset;

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
        const int x = center.x + dot(r, cached.u) * data.fov;
        const int y = center.y + dot(r, cached.v) * data.fov;
        const Point point(x, imageSize.y - y - 1);
        return { { point, data.fov * float(r[H]) } };
    }

    virtual CameraRay unproject(const Point point) const override {
        const float x = (point.x - center.x) / data.fov;
        const float y = ((imageSize.y - point.y - 1) - center.y) / data.fov;
        CameraRay ray;
        ray.origin = cached.u * x + cached.v * y + cached.w * data.zoffset;
        ray.target = ray.origin + cached.w;
        return ray;
    }

    virtual Vector getDirection() const override {
        return cached.w;
    }

    virtual void zoom(const Point fixedPoint, const float magnitude) override {
        ASSERT(magnitude > 0.f);
        center += (fixedPoint - center) * (1.f - magnitude);
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

struct PerspectiveCameraData {
    /// Field of view (angle)
    float fov;

    /// Camera position in space
    Vector position;

    /// Look-at point in space
    Vector target;

    /// Up vector of the camera (direction)
    Vector up;
};

class PerspectiveCamera : public ICamera {
private:
    Point imageSize;
    PerspectiveCameraData data;

    struct {
        /// Unit direction of the camera
        Vector dir;

        /// Up vector of the camera, size of which of represents the image size at unit distance
        Vector up;

        /// Left vector of the camera, size of which of represents the image size at unit distance
        Vector left;

        /// Last matrix set by \ref transform.
        AffineMatrix matrix = AffineMatrix::identity();
    } cached;

public:
    PerspectiveCamera(const Point imageSize, const PerspectiveCameraData& data)
        : imageSize(imageSize)
        , data(data) {
        this->update();
    }

    virtual Optional<ProjectedPoint> project(const Vector& r) const override {
        const Vector dr = r - data.position;
        const Float proj = dot(dr, cached.dir);
        if (proj <= 0._f) {
            // point on the other side of the camera view
            return NOTHING;
        }
        const Vector r0 = dr / proj;
        // convert [-1, 1] to [0, imageSize]
        Vector left0;
        Float leftLength;
        tieToTuple(left0, leftLength) = getNormalizedWithLength(cached.left);
        Vector up0;
        Float upLength;
        tieToTuple(up0, upLength) = getNormalizedWithLength(cached.up);
        const Float leftRel = dot(left0, r0) / leftLength;
        // ASSERT(leftRel >= -1._f && leftRel <= 1._f);
        const Float upRel = dot(up0, r0) / upLength;
        // ASSERT(upRel >= -1._f && upRel <= 1._f);
        const int x = 0.5_f * (1._f + leftRel) * imageSize.x;
        const int y = 0.5_f * (1._f + upRel) * imageSize.y;
        const Float hAtUnitDist = r[H] / proj;
        const Float h = hAtUnitDist / leftLength * imageSize.x;

        // if (x >= -h && x < imageSize.x + h && y >= -h && y < imageSize.y )
        return ProjectedPoint{ { x, imageSize.y - y - 1 }, max(float(h), 1.f) };
    }

    virtual CameraRay unproject(const Point point) const override {
        const Float x = 2._f * Float(point.x) / imageSize.x - 1._f;
        const Float y = 2._f * Float(point.y) / imageSize.y - 1._f;
        CameraRay ray;
        ray.origin = data.position;
        ray.target = ray.origin + cached.dir + cached.left * x - cached.up * y;
        return ray;
    }

    virtual Vector getDirection() const override {
        return cached.dir;
    }

    virtual void zoom(const Point UNUSED(fixedPoint), const float magnitude) override {
        ASSERT(magnitude > 0.f);
        data.fov *= 0.1_f * magnitude;
        this->transform(cached.matrix);
    }

    virtual void transform(const AffineMatrix& matrix) override {
        // reset the previous transform
        this->update();

        cached.dir = matrix * cached.dir;
        cached.up = matrix * cached.up;
        cached.left = matrix * cached.left;
        cached.matrix = matrix;
    }

    virtual void pan(const Point offset) override {
        const Float x = Float(offset.x) / imageSize.x;
        const Float y = Float(offset.y) / imageSize.y;
        const Vector worldOffset = getLength(data.target - data.position) * (cached.left * x + cached.up * y);
        data.position -= worldOffset;
        data.target -= worldOffset;
    }

private:
    void update() {
        cached.dir = getNormalized(data.target - data.position);

        const Float aspect = Float(imageSize.x) / Float(imageSize.y);
        ASSERT(aspect >= 1._f); // not really required, using for simplicity
        const Float tgfov = tan(0.5_f * data.fov);
        cached.up = tgfov / aspect * getNormalized(data.up);
        cached.left = tgfov * getNormalized(cross(cached.up, cached.dir));
    }
};

NAMESPACE_SPH_END
