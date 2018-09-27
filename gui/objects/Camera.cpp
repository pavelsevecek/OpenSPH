#include "gui/objects/Camera.h"
#include "objects/geometry/Box.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// ----------------------------------------------------------------------------------------------------------
/// OrthoCamera
/// ----------------------------------------------------------------------------------------------------------

OrthoCamera::OrthoCamera(const Point imageSize, const Point center, OrthoCameraData data)
    : imageSize(imageSize)
    , center(center)
    , data(data) {
    cached.u = data.u;
    cached.v = data.v;
    cached.w = cross(data.u, data.v);
}

void OrthoCamera::initialize(const Storage& storage) {
    if (data.fov) {
        // fov specified explicitly, we don't have to initialize anything
        return;
    }
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Box box;
    for (Size i = 0; i < r.size(); ++i) {
        box.extend(r[i]);
    }
    data.fov = 0.5f * imageSize.y / maxElement(box.size());
}

Optional<ProjectedPoint> OrthoCamera::project(const Vector& r) const {
    const int x = center.x + dot(r, cached.u) * data.fov.value();
    const int y = center.y + dot(r, cached.v) * data.fov.value();
    const Point point(x, imageSize.y - y - 1);
    return { { point, data.fov.value() * float(r[H]) } };
}

CameraRay OrthoCamera::unproject(const Point point) const {
    const float x = (point.x - center.x) / data.fov.value();
    const float y = ((imageSize.y - point.y - 1) - center.y) / data.fov.value();
    CameraRay ray;
    ray.origin = cached.u * x + cached.v * y + cached.w * data.zoffset;
    ray.target = ray.origin + cached.w;
    return ray;
}

Vector OrthoCamera::getDirection() const {
    return cached.w;
}

void OrthoCamera::zoom(const Point fixedPoint, const float magnitude) {
    ASSERT(magnitude > 0.f);
    center += (fixedPoint - center) * (1.f - magnitude);
    data.fov.value() *= magnitude;
}

void OrthoCamera::transform(const AffineMatrix& matrix) {
    cached.u = matrix * data.u;
    cached.v = matrix * data.v;
    cached.w = cross(cached.u, cached.v);
}

void OrthoCamera::pan(const Point offset) {
    center += offset;
}

/// ----------------------------------------------------------------------------------------------------------
/// PerspectiveCamera
/// ----------------------------------------------------------------------------------------------------------

PerspectiveCamera::PerspectiveCamera(const Point imageSize, const PerspectiveCameraData& data)
    : imageSize(imageSize)
    , data(data) {
    this->update();
}

void PerspectiveCamera::initialize(const Storage& UNUSED(storage)) {
    /// \todo auto-view, like OrthoCamera
}

Optional<ProjectedPoint> PerspectiveCamera::project(const Vector& r) const {
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

CameraRay PerspectiveCamera::unproject(const Point point) const {
    const Float x = 2._f * Float(point.x) / imageSize.x - 1._f;
    const Float y = 2._f * Float(point.y) / imageSize.y - 1._f;
    CameraRay ray;
    ray.origin = data.position;
    ray.target = ray.origin + cached.dir + cached.left * x - cached.up * y;
    return ray;
}

Vector PerspectiveCamera::getDirection() const {
    return cached.dir;
}

void PerspectiveCamera::zoom(const Point UNUSED(fixedPoint), const float magnitude) {
    ASSERT(magnitude > 0.f);
    data.fov *= 0.1_f * magnitude;
    this->transform(cached.matrix);
}

void PerspectiveCamera::transform(const AffineMatrix& matrix) {
    // reset the previous transform
    this->update();

    cached.dir = matrix * cached.dir;
    cached.up = matrix * cached.up;
    cached.left = matrix * cached.left;
    cached.matrix = matrix;
}

void PerspectiveCamera::pan(const Point offset) {
    const Float x = Float(offset.x) / imageSize.x;
    const Float y = Float(offset.y) / imageSize.y;
    const Vector worldOffset = getLength(data.target - data.position) * (cached.left * x + cached.up * y);
    data.position -= worldOffset;
    data.target -= worldOffset;
}

void PerspectiveCamera::update() {
    cached.dir = getNormalized(data.target - data.position);

    const Float aspect = Float(imageSize.x) / Float(imageSize.y);
    ASSERT(aspect >= 1._f); // not really required, using for simplicity
    const Float tgfov = tan(0.5_f * data.fov);
    cached.up = tgfov / aspect * getNormalized(data.up);
    cached.left = tgfov * getNormalized(cross(cached.up, cached.dir));
}

NAMESPACE_SPH_END
