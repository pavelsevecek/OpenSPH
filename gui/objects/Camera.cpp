#include "gui/objects/Camera.h"
#include "objects/geometry/Box.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// ----------------------------------------------------------------------------------------------------------
/// OrthoCamera
/// ----------------------------------------------------------------------------------------------------------

OrthoCamera::OrthoCamera(const Pixel imageSize, const Pixel center, OrthoCameraData data)
    : imageSize(imageSize)
    , center(center)
    , data(data) {
    cached.u = data.u;
    cached.v = data.v;
    cached.w = cross(data.u, data.v);
}

void OrthoCamera::initialize(const Storage& storage) {
    if (data.fov) {
        // fov either specified explicitly or already computed
        return;
    }


    /// \todo also auto-center?

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    Float m_sum = 0._f;
    Vector r_com(0._f);

    for (Size i = 0; i < r.size(); ++i) {
        m_sum += m[i];
        r_com += m[i] * r[i];
    }
    r_com /= m_sum;

    Array<Float> distances(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        const Vector dr = r[i] - r_com;
        distances[i] = getLength(dr - cached.w * dot(cached.w, dr));
    }
    // find median distance
    const Size mid = r.size() / 2;
    std::nth_element(distances.begin(), distances.begin() + mid, distances.end());
    // factor 5 is ad hoc
    const Float fov = 5._f * distances[mid];
    ASSERT(fov > EPS);

    data.fov = imageSize.y / fov;
}

Optional<ProjectedPoint> OrthoCamera::project(const Vector& r) const {
    const float x = center.x + dot(r, cached.u) * data.fov.value();
    const float y = center.y + dot(r, cached.v) * data.fov.value();
    const Coords point(x, imageSize.y - y - 1);
    return { { point, data.fov.value() * float(r[H]) } };
}

CameraRay OrthoCamera::unproject(const Coords& coords) const {
    const float rx = (coords.x - center.x) / data.fov.value();
    const float ry = ((imageSize.y - coords.y - 1) - center.y) / data.fov.value();
    CameraRay ray;
    ray.origin = cached.u * rx + cached.v * ry + cached.w * data.zoffset;
    ray.target = ray.origin + cached.w;
    return ray;
}

Vector OrthoCamera::getDirection() const {
    return cached.w;
}

Optional<float> OrthoCamera::getCutoff() const {
    return data.cutoff;
}

void OrthoCamera::setCutoff(const Optional<float> newCutoff) {
    data.cutoff = newCutoff;
}

void OrthoCamera::zoom(const Pixel fixedPoint, const float magnitude) {
    ASSERT(magnitude > 0.f);
    center += (fixedPoint - center) * (1.f - magnitude);
    data.fov.value() *= magnitude;
}

void OrthoCamera::transform(const AffineMatrix& matrix) {
    cached.u = matrix * data.u;
    cached.v = matrix * data.v;
    cached.w = cross(cached.u, cached.v);
}

void OrthoCamera::pan(const Pixel offset) {
    center += offset;
}

/// ----------------------------------------------------------------------------------------------------------
/// PerspectiveCamera
/// ----------------------------------------------------------------------------------------------------------

Vector ParticleTracker::position(const Storage& storage) const {
    if (index < storage.getParticleCnt()) {
        return storage.getValue<Vector>(QuantityId::POSITION)[index];
    } else {
        return Vector(0._f);
    }
}

PerspectiveCamera::PerspectiveCamera(const Pixel imageSize, const PerspectiveCameraData& data)
    : imageSize(imageSize)
    , data(data) {
    ASSERT(data.clipping.lower() > 0._f && data.clipping.size() > EPS);

    this->update();
}

void PerspectiveCamera::initialize(const Storage& storage) {
    /// \todo auto-view, like OrthoCamera ?

    if (data.tracker) {
        const Vector newTarget = data.tracker->position(storage);
        data.position += newTarget - data.target;
        data.target = newTarget;
        this->update();
    }
}

Optional<ProjectedPoint> PerspectiveCamera::project(const Vector& r) const {
    const Vector dr = r - data.position;
    const Float proj = dot(dr, cached.dir);
    if (!data.clipping.contains(proj)) {
        // point clipped by the clipping planes
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
    const float leftRel = dot(left0, r0) / leftLength;
    const float upRel = dot(up0, r0) / upLength;
    const float x = 0.5f * (1.f + leftRel) * imageSize.x;
    const float y = 0.5f * (1.f + upRel) * imageSize.y;
    const float hAtUnitDist = r[H] / proj;
    const float h = hAtUnitDist / leftLength * imageSize.x;

    // if (x >= -h && x < imageSize.x + h && y >= -h && y < imageSize.y )
    return ProjectedPoint{ { x, imageSize.y - y - 1 }, max(float(h), 1.f) };
}

CameraRay PerspectiveCamera::unproject(const Coords& coords) const {
    const float rx = 2.f * coords.x / imageSize.x - 1.f;
    const float ry = 2.f * coords.y / imageSize.y - 1.f;
    CameraRay ray;
    ray.origin = data.position;
    ray.target = ray.origin + cached.dir + cached.left * rx - cached.up * ry;
    return ray;
}

Vector PerspectiveCamera::getDirection() const {
    return cached.dir;
}

Optional<float> PerspectiveCamera::getCutoff() const {
    // not implemented yet
    return NOTHING;
}

void PerspectiveCamera::setCutoff(const Optional<float> UNUSED(newCutoff)) {}

void PerspectiveCamera::zoom(const Pixel UNUSED(fixedPoint), const float magnitude) {
    ASSERT(magnitude > 0.f);
    data.fov *= 1._f + 0.01_f * magnitude;
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

void PerspectiveCamera::pan(const Pixel offset) {
    const Float x = Float(offset.x) / imageSize.x;
    const Float y = Float(offset.y) / imageSize.y;
    const Vector worldOffset = getLength(data.target - data.position) * (cached.left * x + cached.up * y);
    data.position -= worldOffset;
    data.target -= worldOffset;
}

void PerspectiveCamera::update() {
    cached.dir = getNormalized(data.target - data.position);

    // make sure the up vector is perpendicular
    Vector up = data.up;
    up = getNormalized(up - dot(up, cached.dir) * cached.dir);
    ASSERT(abs(dot(up, cached.dir)) < EPS);

    const Float aspect = Float(imageSize.x) / Float(imageSize.y);
    ASSERT(aspect >= 1._f); // not really required, using for simplicity
    const Float tgfov = tan(0.5_f * data.fov);
    cached.up = tgfov / aspect * up;
    cached.left = tgfov * getNormalized(cross(cached.up, cached.dir));
}

NAMESPACE_SPH_END
