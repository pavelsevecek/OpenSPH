#include "gui/objects/Camera.h"
#include "objects/containers/ArrayRef.h"
#include "objects/geometry/Box.h"
#include "quantities/Quantity.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

Pair<Vector> ParticleTracker::getCameraState(const Storage& storage) const {
    if (index < storage.getParticleCnt()) {
        const Quantity& pos = storage.getQuantity(QuantityId::POSITION);
        return { pos.getValue<Vector>()[index], pos.getDt<Vector>()[index] };
    } else {
        return { Vector(0._f), Vector(0._f) };
    }
}

Pair<Vector> MedianTracker::getCameraState(const Storage& storage) const {
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    Array<Float> x(r.size());
    Array<Float> y(r.size());
    Array<Float> z(r.size());
    for (Size i = 0; i < r.size(); ++i) {
        x[i] = r[i][X];
        y[i] = r[i][Y];
        z[i] = r[i][Z];
    }
    const Size mid = r.size() / 2;
    std::nth_element(x.begin(), x.begin() + mid, x.end());
    std::nth_element(y.begin(), y.begin() + mid, y.end());
    std::nth_element(z.begin(), z.begin() + mid, z.end());
    return { Vector(x[mid], y[mid], z[mid]) + offset, Vector(0._f) };
}


// ----------------------------------------------------------------------------------------------------------
// OrthoCamera
// ----------------------------------------------------------------------------------------------------------

OrthoCamera::OrthoCamera(const CameraData& data)
    : data(data) {
    this->update();
}

void OrthoCamera::update() {
    cached.w = getNormalized(data.target - data.position);
    cached.v = getNormalized(data.up);
    cached.v -= dot(cached.v, cached.w) * cached.w;
    cached.u = cross(cached.v, cached.w);
}

void OrthoCamera::initialize(const Storage& storage) {
    if (data.ortho.fov) {
        // fov either specified explicitly or already computed
        return;
    }

    /// \todo also auto-center?
    // handle case without mass in storage
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    ArrayRef<const Float> m;
    if (storage.has(QuantityId::MASS)) {
        m = makeArrayRef(storage.getValue<Float>(QuantityId::MASS), RefEnum::WEAK);
    } else {
        Array<Float> dummy(r.size());
        dummy.fill(1._f);
        m = makeArrayRef(std::move(dummy), RefEnum::STRONG);
    }

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

    data.ortho.fov = data.imageSize.y / fov;
}

Optional<ProjectedPoint> OrthoCamera::project(const Vector& r) const {
    if (!data.ortho.fov) {
        return NOTHING;
    }
    const float fov = data.ortho.fov.value();
    const float x = dot(r - data.position, cached.u) * fov;
    const float y = dot(r - data.position, cached.v) * fov;
    const Coords point = Coords(data.imageSize.x / 2 + x, data.imageSize.y / 2 - y - 1);
    return { { point, fov * float(r[H]) } };
}

Optional<CameraRay> OrthoCamera::unproject(const Coords& coords) const {
    if (!data.ortho.fov) {
        return CameraRay{ Vector(0._f), cached.w };
    }
    const float fov = data.ortho.fov.value();
    const float rx = (coords.x - data.imageSize.x * 0.5f) / fov;
    const float ry = (data.imageSize.y * 0.5f - coords.y - 1) / fov;
    CameraRay ray;
    ray.origin = data.position + cached.u * rx + cached.v * ry + cached.w * data.clipping.lower();
    ray.target = ray.origin + cached.w;
    return ray;
}

Vector OrthoCamera::getDirection() const {
    return cached.w;
}

Optional<float> OrthoCamera::getCutoff() const {
    return data.ortho.cutoff;
}

Optional<float> OrthoCamera::getWorldToPixel() const {
    return data.ortho.fov;
}

void OrthoCamera::setCutoff(const Optional<float> newCutoff) {
    data.ortho.cutoff = newCutoff;
}

void OrthoCamera::zoom(const Pixel fixedPoint, const float magnitude) {
    ASSERT(magnitude > 0.f);
    if (!data.ortho.fov) {
        // this can be called before run starts
        return;
    }

    const Vector fixed1 = this->unproject(Coords(fixedPoint))->origin;
    data.ortho.fov.value() *= magnitude;
    const Vector fixed2 = this->unproject(Coords(fixedPoint))->origin;
    const Vector dp = fixed1 - fixed2;
    data.position += dp;
    data.target += dp;
}

void OrthoCamera::transform(const AffineMatrix& matrix) {
    this->update();
    cached.u = matrix * cached.u;
    cached.v = matrix * cached.v;
    cached.w = cross(cached.u, cached.v);
}

void OrthoCamera::pan(const Pixel offset) {
    if (!data.ortho.fov) {
        // this can be called before run starts
        return;
    }

    const float fov = data.ortho.fov.value();
    const Vector dp = -offset.x / fov * cached.u - offset.y / fov * cached.v;
    data.position += dp;
    data.target += dp;
}

void OrthoCamera::resize(const Pixel newSize) {
    // const Coords scaling = Coords(newSize) / Coords(data.imageSize);
    data.imageSize = newSize;
    // center = Pixel(Coords(center) * scaling);
}

// ----------------------------------------------------------------------------------------------------------
// PerspectiveCamera
// ----------------------------------------------------------------------------------------------------------

PerspectiveCamera::PerspectiveCamera(const CameraData& data)
    : data(data) {
    ASSERT(data.clipping.lower() > 0._f && data.clipping.size() > EPS);

    this->update();
}

void PerspectiveCamera::initialize(const Storage& storage) {
    if (data.tracker) {
        Vector newTarget;
        tie(newTarget, cached.velocity) = data.tracker->getCameraState(storage);
        data.position += newTarget - data.target;
        data.target = newTarget;
        this->update();
    } else {
        cached.velocity = Vector(0._f);
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
    const float x = 0.5f * (1.f + leftRel) * data.imageSize.x;
    const float y = 0.5f * (1.f + upRel) * data.imageSize.y;
    const float hAtUnitDist = r[H] / proj;
    const float h = hAtUnitDist / leftLength * data.imageSize.x;

    // if (x >= -h && x < imageSize.x + h && y >= -h && y < imageSize.y )
    return ProjectedPoint{ { x, data.imageSize.y - y - 1 }, max(float(h), 1.f) };
}

Optional<CameraRay> PerspectiveCamera::unproject(const Coords& coords) const {
    const float rx = 2.f * coords.x / data.imageSize.x - 1.f;
    const float ry = 2.f * coords.y / data.imageSize.y - 1.f;
    const Vector dir = cached.dir + cached.left * rx - cached.up * ry;
    CameraRay ray;
    /// \todo implement far clipping plane (max ray param)
    ray.origin = data.position + data.clipping.lower() * dir;
    ray.target = ray.origin + dir;
    return ray;
}

Vector PerspectiveCamera::getDirection() const {
    return cached.dir;
}

Optional<float> PerspectiveCamera::getCutoff() const {
    // not implemented yet
    return NOTHING;
}

Optional<float> PerspectiveCamera::getWorldToPixel() const {
    return NOTHING;
}

void PerspectiveCamera::setCutoff(const Optional<float> UNUSED(newCutoff)) {}

void PerspectiveCamera::zoom(const Pixel UNUSED(fixedPoint), const float magnitude) {
    ASSERT(magnitude > 0.f);
    // data.perspective.fov *= 1._f + 0.01_f * magnitude;
    // this->transform(//cached.matrix);
}

void PerspectiveCamera::transform(const AffineMatrix& matrix) {
    // reset the previous transform
    this->update();

    cached.dir = matrix * cached.dir;
    cached.up = matrix * cached.up;
    cached.left = matrix * cached.left;
}

void PerspectiveCamera::pan(const Pixel offset) {
    const Float x = Float(offset.x) / data.imageSize.x;
    const Float y = Float(offset.y) / data.imageSize.y;
    const Vector worldOffset = getLength(data.target - data.position) * (cached.left * x + cached.up * y);
    data.position -= worldOffset;
    data.target -= worldOffset;
}

void PerspectiveCamera::resize(const Pixel newSize) {
    data.imageSize = newSize;
    this->update();
}

void PerspectiveCamera::update() {
    cached.dir = getNormalized(data.target - data.position);

    // make sure the up vector is perpendicular
    Vector up = data.up;
    up = getNormalized(up - dot(up, cached.dir) * cached.dir);
    ASSERT(abs(dot(up, cached.dir)) < EPS);

    const Float aspect = Float(data.imageSize.x) / Float(data.imageSize.y);
    ASSERT(aspect >= 1._f); // not really required, using for simplicity
    const Float tgfov = tan(0.5_f * data.perspective.fov);
    cached.up = tgfov / aspect * up;
    cached.left = tgfov * getNormalized(cross(cached.up, cached.dir));
}

// ----------------------------------------------------------------------------------------------------------
// PanoCameraBase
// ----------------------------------------------------------------------------------------------------------

PanoCameraBase::PanoCameraBase(const CameraData& data)
    : data(data) {
    ASSERT(data.clipping.lower() > 0._f && data.clipping.size() > EPS);
}

void PanoCameraBase::initialize(const Storage& storage) {
    if (data.tracker) {
        const Vector newTarget = data.tracker->getCameraState(storage)[0];
        data.position += newTarget - data.target;
        data.target = newTarget;
        this->update();
    }
}

Optional<ProjectedPoint> PanoCameraBase::project(const Vector& UNUSED(r)) const {
    /// \todo
    return NOTHING;
}

Vector PanoCameraBase::getDirection() const {
    return getNormalized(data.target - data.position);
}

Optional<float> PanoCameraBase::getCutoff() const {
    // not implemented yet
    return NOTHING;
}

Optional<float> PanoCameraBase::getWorldToPixel() const {
    return NOTHING;
}

void PanoCameraBase::setCutoff(const Optional<float> UNUSED(newCutoff)) {}

void PanoCameraBase::zoom(const Pixel UNUSED(fixedPoint), const float UNUSED(magnitude)) {
    NOT_IMPLEMENTED;
}

void PanoCameraBase::transform(const AffineMatrix& UNUSED(matrix)) {
    NOT_IMPLEMENTED;
}

void PanoCameraBase::pan(const Pixel UNUSED(offset)) {
    NOT_IMPLEMENTED;
}

void PanoCameraBase::resize(const Pixel newSize) {
    data.imageSize = newSize;
    this->update();
}

void PanoCameraBase::update() {
    const Vector dir = getNormalized(data.target - data.position);

    // make sure the up vector is perpendicular
    const Vector up = getNormalized(data.up - dot(data.up, dir) * dir);
    ASSERT(abs(dot(up, dir)) < EPS);

    const Vector left = getNormalized(cross(up, dir));

    matrix = AffineMatrix(-dir, -left, up).inverse();
}

// ----------------------------------------------------------------------------------------------------------
// FisheyeCamera
// ----------------------------------------------------------------------------------------------------------

FisheyeCamera::FisheyeCamera(const CameraData& data)
    : PanoCameraBase(data) {
    this->update();
}

Optional<CameraRay> FisheyeCamera::unproject(const Coords& coords) const {
    const Coords p = (coords - cached.center) / cached.radius;
    const Float r = getLength(p);
    if (r > 1._f) {
        return NOTHING;
    }

    const Float theta = r * PI / 2._f;
    const Float phi = atan2(p.y, p.x) + PI / 2._f;

    const Vector localDir = sphericalToCartesian(1._f, theta, phi);
    const Vector dir = matrix * localDir;

    CameraRay ray;
    ray.origin = data.position + dir * data.clipping.lower();
    ray.target = ray.origin + dir;
    return ray;
}

void FisheyeCamera::update() {
    const Vector dir = getNormalized(data.target - data.position);

    // make sure the up vector is perpendicular
    const Vector up = getNormalized(data.up - dot(data.up, dir) * dir);
    ASSERT(abs(dot(up, dir)) < EPS);

    const Vector left = getNormalized(cross(up, dir));

    matrix = AffineMatrix(up, left, dir).inverse();

    cached.center = Coords(data.imageSize.x / 2, data.imageSize.y / 2);
    cached.radius = min(cached.center.x, cached.center.y);
}

// ----------------------------------------------------------------------------------------------------------
// SphericalCamera
// ----------------------------------------------------------------------------------------------------------

SphericalCamera::SphericalCamera(const CameraData& data)
    : PanoCameraBase(data) {
    this->update();
}

Optional<CameraRay> SphericalCamera::unproject(const Coords& coords) const {
    const Float phi = 2._f * PI * coords.x / data.imageSize.x;
    const Float theta = PI * coords.y / data.imageSize.y;
    const Vector dir = matrix * sphericalToCartesian(1._f, theta, phi);

    CameraRay ray;
    ray.origin = data.position + dir * data.clipping.lower();
    ray.target = ray.origin + dir;
    return ray;
}

NAMESPACE_SPH_END
