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

class ITracker : public Polymorphic {
public:
    // position and velocity
    virtual Pair<Vector> getTrackedPoint(const Storage& storage) const = 0;
};

class ParticleTracker : public ITracker {
private:
    Size index;

public:
    explicit ParticleTracker(const Size index)
        : index(index) {}

    virtual Pair<Vector> getTrackedPoint(const Storage& storage) const override;
};

class MedianTracker : public ITracker {
private:
    Vector offset;

public:
    explicit MedianTracker(const Vector& offset)
        : offset(offset) {}

    virtual Pair<Vector> getTrackedPoint(const Storage& storage) const override;
};

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
    virtual void autoSetup(const Storage& storage) = 0;

    /// \brief Returns projected position of particle on the image.
    ///
    /// If the particle is outside of the image region or is clipped by the projection, returns NOTHING.
    virtual Optional<ProjectedPoint> project(const Vector& r) const = 0;

    /// \brief Returns a ray in particle coordinates corresponding to given coordinates in the image plane.
    virtual Optional<CameraRay> unproject(const Coords& coords) const = 0;

    /// \brief Returns the current resolution of the camera
    virtual Pixel getSize() const = 0;

    /// \brief Returns the transformation matrix converting camera space to world space.
    ///
    /// In the camera space, camera direction is aligned with the z-axis, y-axis corresponds to the up-vector
    /// and x-axis is perpendicular, i.e. left-vector.
    virtual AffineMatrix getFrame() const = 0;

    virtual Vector getTarget() const = 0;

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

    /// \brief Moves the camera to new position in world space.
    virtual void setPosition(const Vector& newPosition) = 0;

    virtual void setTarget(const Vector& newTarget) = 0;

    /// \brief Transforms the current view by given matrix.
    ///
    /// This replaces previous transformation matrix, i.e. subsequent calls do not accumulate.
    /// \param matrix Transform matrix applied to the camera.
    virtual void transform(const AffineMatrix& matrix) = 0;

    /// \brief Moves the camera by relative offset in image space.
    virtual void pan(const Pixel offset) = 0;

    /// \brief Changes the image size.
    virtual void resize(const Pixel newSize) = 0;

    /// \todo revert to ClonePtr!
    virtual AutoPtr<ICamera> clone() const = 0;
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

    struct {
        /// Field of view (angle)
        Float fov = PI / 3._f;
    } perspective;

    struct {

        /// Field of view (world units).
        float fov = 1.e5_f;

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

    virtual void autoSetup(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual Optional<CameraRay> unproject(const Coords& coords) const override;

    virtual Pixel getSize() const override;

    virtual AffineMatrix getFrame() const override;

    virtual Vector getTarget() const override;

    virtual Optional<float> getCutoff() const override;

    virtual Optional<float> getWorldToPixel() const override;

    virtual void setCutoff(const Optional<float> newCutoff) override;

    virtual void zoom(const Pixel fixedPoint, const float magnitude) override;

    virtual void setPosition(const Vector& newPosition) override;

    virtual void setTarget(const Vector& newTarget) override;

    virtual void transform(const AffineMatrix& matrix) override;

    virtual void pan(const Pixel offset) override;

    virtual void resize(const Pixel newSize) override;

    virtual AutoPtr<ICamera> clone() const override {
        return makeAuto<OrthoCamera>(*this);
    }

private:
    void update();

    Optional<CameraRay> unprojectImpl(const Coords& coords, const bool adjustZ) const;

    float estimateFov(const Storage& storage) const;
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

    virtual void autoSetup(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual Optional<CameraRay> unproject(const Coords& coords) const override;

    virtual Pixel getSize() const override;

    virtual AffineMatrix getFrame() const override;

    virtual Vector getTarget() const override;

    virtual Optional<float> getCutoff() const override;

    virtual Optional<float> getWorldToPixel() const override;

    virtual void setCutoff(const Optional<float> newCutoff) override;

    virtual void zoom(const Pixel UNUSED(fixedPoint), const float magnitude) override;

    virtual void setPosition(const Vector& newPosition) override;

    virtual void setTarget(const Vector& newTarget) override;

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

    virtual void autoSetup(const Storage& storage) override;

    virtual Optional<ProjectedPoint> project(const Vector& r) const override;

    virtual Pixel getSize() const override;

    virtual AffineMatrix getFrame() const override;

    virtual Vector getTarget() const override;

    virtual Optional<float> getCutoff() const override;

    virtual Optional<float> getWorldToPixel() const override;

    virtual void setCutoff(const Optional<float> newCutoff) override;

    virtual void zoom(const Pixel UNUSED(fixedPoint), const float magnitude) override;

    virtual void setPosition(const Vector& newPosition) override;

    virtual void setTarget(const Vector& newTarget) override;

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

        float radius;
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
