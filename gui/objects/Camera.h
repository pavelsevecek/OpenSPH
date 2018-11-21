#pragma once

/// \file Camera.h
/// \brief Defines projection transforming 3D particles onto 2D screen
/// \author Pavel Sevecek (sevecek at sirrah.troja.mf.cuni.cz)
/// \date 2016-2018

#include "gui/objects/Point.h"
#include "math/AffineMatrix.h"
#include "objects/wrappers/ClonePtr.h"
#include "objects/wrappers/Optional.h"

NAMESPACE_SPH_BEGIN

class Storage;

/// \brief Represents a particle projected onto image plane
struct ProjectedPoint {

    /// Point in image coordinates
    Coords coords;

    /// Projected radius of the particle
    float radius;
};

/// \brief Ray given by origin and target point
struct CameraRay {
    Vector origin;
    Vector target;
};

/// \brief Interface defining a camera or view, used by a renderer.
class ICamera : public Polymorphic {
public:
    /// \brief Initializes the camera, using the provided particle storage.
    ///
    /// Called every time step. Useful if camera co-moves with the particles, the field of view changes during
    /// the simulation, etc. If the camera does not depend on particle positions or any quantity, function can
    /// be empty.
    virtual void initialize(const Storage& storage) = 0;

    /// \brief Returns projected position of particle on the image.
    ///
    /// If the particle is outside of the image region or is clipped by the projection, returns NOTHING.
    virtual Optional<ProjectedPoint> project(const Vector& r) const = 0;

    /// \brief Returns a ray in particle coordinates corresponding to given coordinates in the image plane.
    virtual CameraRay unproject(const Coords& coords) const = 0;

    /// \brief Returns the direction of the camera.
    virtual Vector getDirection() const = 0;

    /// \brief Returns the clipping distance from plane passing through origin, perpendicular to camera
    /// direction.
    ///
    /// If no clipping is used, the function returns NOTHING. Useful to view a section through a body rather
    /// than its surface.
    virtual Optional<float> getCutoff() const = 0;

    /// \param Applies zoom to the camera.
    ///
    /// This is usually equivalent to transforming the view with scaling matrix, alhough it can be implemented
    /// differently.
    /// \param fixedPoint Point that remains fixed after the zoom (for magnitude != 1, there is exactly one)
    /// \param magnitude Relative zoom amount, value <1 means zooming out, value >1 means zooming in.
    virtual void zoom(const Pixel fixedPoint, const float magnitude) = 0;

    /// \brief Transforms the current view by given matrix.
    ///
    /// This replaces previous transformation matrix, i.e. subsequent calls do not accumulate.
    /// \param matrix Transform matrix applied to the camera.
    virtual void transform(const AffineMatrix& matrix) = 0;

    /// \brief Moves the camera by relative offset
    virtual void pan(const Pixel offset) = 0;

    /// \todo revert to ClonePtr!
    virtual AutoPtr<ICamera> clone() const = 0;
};


struct OrthoCameraData {
    /// Field of view (zoom)
    Optional<float> fov = NOTHING;

    /// Cutoff distance of the camera.
    Optional<float> cutoff = NOTHING;

    /// Z-offset of the camera
    float zoffset = 0.f;

    /// Vectors defining camera plane
    Vector u = Vector(1._f, 0._f, 0._f);

    Vector v = Vector(0._f, 1._f, 0._f);
};

/// \brief Orthographic camera.
class OrthoCamera : public ICamera {
private:
    Pixel imageSize;
    Pixel center;

    OrthoCameraData data;

    /// Cached transformed values
    struct {
        Vector u, v, w;
    } cached;

public:
    OrthoCamera(const Pixel imageSize, const Pixel center, OrthoCameraData data);

    void setCutoff(const Optional<float> cutoff) {
        data.cutoff = cutoff;
    }

    virtual void initialize(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual CameraRay unproject(const Coords& coords) const override;

    virtual Vector getDirection() const override;

    virtual Optional<float> getCutoff() const override;

    virtual void zoom(const Pixel fixedPoint, const float magnitude) override;

    virtual void transform(const AffineMatrix& matrix) override;

    virtual void pan(const Pixel offset) override;

    virtual AutoPtr<ICamera> clone() const override {
        return makeAuto<OrthoCamera>(*this);
    }
};

class ITracker : public Polymorphic {
public:
    virtual Vector position(const Storage& storage) const = 0;
};

class ConstTracker : public ITracker {
private:
    Vector pos;

public:
    explicit ConstTracker(const Vector& pos)
        : pos(pos) {}

    virtual Vector position(const Storage& UNUSED(storage)) const override {
        return pos;
    }
};

class ParticleTracker : public ITracker {
private:
    Size index;

public:
    explicit ParticleTracker(const Size index)
        : index(index) {}

    virtual Vector position(const Storage& storage) const override;
};

struct PerspectiveCameraData {
    /// Field of view (angle)
    Float fov = PI / 3._f;

    /// Camera position in space
    Vector position = Vector(0._f, 0._f, -1._f);

    /// Look-at point in space
    Vector target = Vector(0._f);

    /// Up vector of the camera (direction)
    Vector up = Vector(0._f, 1._f, 0._f);

    /// Defines the clipping planes of the camera.
    Interval clipping = Interval(0._f, INFTY);

    ClonePtr<ITracker> tracker = nullptr;
};

/// \brief Perspective camera
class PerspectiveCamera : public ICamera {
private:
    Pixel imageSize;
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
    PerspectiveCamera(const Pixel imageSize, const PerspectiveCameraData& data);

    virtual void initialize(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual CameraRay unproject(const Coords& coords) const override;

    virtual Vector getDirection() const override;

    virtual Optional<float> getCutoff() const override;

    virtual void zoom(const Pixel UNUSED(fixedPoint), const float magnitude) override;

    virtual void transform(const AffineMatrix& matrix) override;

    virtual void pan(const Pixel offset) override;

    virtual AutoPtr<ICamera> clone() const override {
        return makeAuto<PerspectiveCamera>(*this);
    }

private:
    void update();
};

NAMESPACE_SPH_END
