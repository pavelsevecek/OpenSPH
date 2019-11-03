#pragma once

/// \file Camera.h
/// \brief Defines projection transforming 3D particles onto 2D screen
/// \author Pavel Sevecek (sevecek at sirrah.troja.mf.cuni.cz)
/// \date 2016-2019

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
    virtual Optional<CameraRay> unproject(const Coords& coords) const = 0;

    /// \brief Returns the direction of the camera.
    virtual Vector getDirection() const = 0;

    virtual Vector getVelocity() const = 0;

    /// \brief Returns the clipping distance from plane passing through origin, perpendicular to camera
    /// direction.
    ///
    /// If no clipping is used, the function returns NOTHING. Useful to view a section through a body rather
    /// than its surface.
    virtual Optional<float> getCutoff() const = 0;

    /// \brief Returns the world-to-pixel ratio.
    virtual Optional<float> getWorldToPixel() const = 0;

    /// \brief Modifies the clipping distance of the camera.
    ///
    /// The clipping can be disabled by passing NOTHING.
    virtual void setCutoff(const Optional<float> newCutoff) = 0;

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

    /// \brief Changes the image size.
    virtual void resize(const Pixel newSize) = 0;

    /// \todo revert to ClonePtr!
    virtual AutoPtr<ICamera> clone() const = 0;
};

class ITracker : public Polymorphic {
public:
    // position and velocity
    virtual Pair<Vector> getCameraState(const Storage& storage) const = 0;
};

class ParticleTracker : public ITracker {
private:
    Size index;

public:
    explicit ParticleTracker(const Size index)
        : index(index) {}

    virtual Pair<Vector> getCameraState(const Storage& storage) const override;
};

class MedianTracker : public ITracker {
private:
    Vector offset;

public:
    explicit MedianTracker(const Vector& offset)
        : offset(offset) {}

    virtual Pair<Vector> getCameraState(const Storage& storage) const override;
};

struct CameraData {
    /// Size of the image
    Pixel imageSize = Pixel(1024, 768);

    /// Camera position in space
    Vector position = Vector(0._f, 0._f, -1._f);

    /// Look-at point in space
    Vector target = Vector(0._f);

    /// Up vector of the camera (direction)
    Vector up = Vector(0._f, 1._f, 0._f);

    /// Defines the clipping planes of the camera.
    Interval clipping = Interval(EPS, INFTY);

    /// Object returning position of the camera based on current particle state.
    ClonePtr<ITracker> tracker = nullptr;

    struct {
        /// Field of view (angle)
        Float fov = PI / 3._f;
    } perspective;

    struct {

        /// Field of view (world units). NOTHING means automatic.
        Optional<float> fov = NOTHING;

        /// Cutoff distance of the camera.
        Optional<float> cutoff = NOTHING;

    } ortho;
};


/// \brief Orthographic camera.
class OrthoCamera : public ICamera {
private:
    CameraData data;

    /// Cached transformed values
    struct {
        Vector u, v, w;
    } cached;

public:
    OrthoCamera(const CameraData& data);

    virtual void initialize(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual Optional<CameraRay> unproject(const Coords& coords) const override;

    virtual Vector getDirection() const override;

    virtual Vector getVelocity() const override {
        return Vector(0._f);
    }

    virtual Optional<float> getCutoff() const override;

    virtual Optional<float> getWorldToPixel() const override;

    virtual void setCutoff(const Optional<float> newCutoff) override;

    virtual void zoom(const Pixel fixedPoint, const float magnitude) override;

    virtual void transform(const AffineMatrix& matrix) override;

    virtual void pan(const Pixel offset) override;

    virtual void resize(const Pixel newSize) override;

    virtual AutoPtr<ICamera> clone() const override {
        return makeAuto<OrthoCamera>(*this);
    }

private:
    void update();
};

/// \brief Perspective camera
class PerspectiveCamera : public ICamera {
private:
    CameraData data;

    struct {
        /// Unit direction of the camera
        Vector dir;

        /// Up vector of the camera, size of which of represents the image size at unit distance
        Vector up;

        /// Left vector of the camera, size of which of represents the image size at unit distance
        Vector left;

        Vector velocity;

    } cached;

public:
    PerspectiveCamera(const CameraData& data);

    virtual void initialize(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual Optional<CameraRay> unproject(const Coords& coords) const override;

    virtual Vector getDirection() const override;

    virtual Vector getVelocity() const override {
        return cached.velocity;
    }

    virtual Optional<float> getCutoff() const override;

    virtual Optional<float> getWorldToPixel() const override;

    virtual void setCutoff(const Optional<float> newCutoff) override;

    virtual void zoom(const Pixel UNUSED(fixedPoint), const float magnitude) override;

    virtual void transform(const AffineMatrix& matrix) override;

    virtual void pan(const Pixel offset) override;

    virtual void resize(const Pixel newSize) override;

    virtual AutoPtr<ICamera> clone() const override {
        return makeAuto<PerspectiveCamera>(*this);
    }

private:
    void update();
};

/// \brief Common base for panoramic cameras.
class PanoCameraBase : public ICamera {
protected:
    CameraData data;
    AffineMatrix matrix;

public:
    explicit PanoCameraBase(const CameraData& data);

    virtual void initialize(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual Vector getDirection() const override;

    virtual Vector getVelocity() const override {
        return Vector(0._f);
    }

    virtual Optional<float> getCutoff() const override;

    virtual Optional<float> getWorldToPixel() const override;

    virtual void setCutoff(const Optional<float> newCutoff) override;

    virtual void zoom(const Pixel UNUSED(fixedPoint), const float magnitude) override;

    virtual void transform(const AffineMatrix& matrix) override;

    virtual void pan(const Pixel offset) override;

    virtual void resize(const Pixel newSize) override;

protected:
    virtual void update();
};

/// \brief Fisheye camera
class FisheyeCamera : public PanoCameraBase {
private:
    struct {
        Coords center;

        Size radius;
    } cached;

public:
    explicit FisheyeCamera(const CameraData& data);

    virtual Optional<CameraRay> unproject(const Coords& coords) const override;

    virtual AutoPtr<ICamera> clone() const override {
        return makeAuto<FisheyeCamera>(*this);
    }

private:
    virtual void update() override;
};

/// \brief Spherical camera
class SphericalCamera : public PanoCameraBase {
public:
    explicit SphericalCamera(const CameraData& data);

    virtual Optional<CameraRay> unproject(const Coords& coords) const override;

    virtual AutoPtr<ICamera> clone() const override {
        return makeAuto<SphericalCamera>(*this);
    }
};


NAMESPACE_SPH_END
