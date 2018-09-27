#pragma once

/// \file Camera.h
/// \brief Defines projection transforming 3D particles onto 2D screen
/// \author Pavel Sevecek (sevecek at sirrah.troja.mf.cuni.cz)
/// \date 2016-2018

#include "gui/objects/Point.h"
#include "math/AffineMatrix.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

class Storage;

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
    /// \brief Initializes the camera, using the provided particle storage.
    ///
    /// Called once at the beginning of the simulation. If the camera does not depend on particle positions or
    /// any quantity, function can be empty.
    virtual void initialize(const Storage& storage) = 0;

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
    Optional<float> fov = NOTHING;

    /// Z-offset of the camera
    float zoffset = 0.f;

    /// Vectors defining camera plane
    Vector u = Vector(1._f, 0._f, 0._f);

    Vector v = Vector(0._f, 1._f, 0._f);
};

/// \brief Orthographic camera.
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
    OrthoCamera(const Point imageSize, const Point center, OrthoCameraData data);

    virtual void initialize(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual CameraRay unproject(const Point point) const override;

    virtual Vector getDirection() const override;

    virtual void zoom(const Point fixedPoint, const float magnitude) override;

    virtual void transform(const AffineMatrix& matrix) override;

    virtual void pan(const Point offset) override;
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

/// \brief Perspective camera
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
    PerspectiveCamera(const Point imageSize, const PerspectiveCameraData& data);

    virtual void initialize(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual CameraRay unproject(const Point point) const override;

    virtual Vector getDirection() const override;

    virtual void zoom(const Point UNUSED(fixedPoint), const float magnitude) override;

    virtual void transform(const AffineMatrix& matrix) override;

    virtual void pan(const Point offset) override;

private:
    void update();
};

NAMESPACE_SPH_END
