#include "gui/objects/Camera.h"
#include "objects/containers/ArrayRef.h"
#include "objects/geometry/Box.h"
#include "quantities/Storage.h"

NAMESPACE_SPH_BEGIN

/// ----------------------------------------------------------------------------------------------------------
/// OrthoCamera
/// ----------------------------------------------------------------------------------------------------------

OrthoCamera::OrthoCamera(const CameraData& data)
    : data(data) {
    cached.imageSize = data.imageSize;
    cached.position = data.position;
    cached.cutoff = data.ortho.cutoff;

    this->reset();

    if (data.ortho.fov) {
        cached.worldToPixel = data.imageSize.y / data.ortho.fov.value();
    }
}

void OrthoCamera::initialize(const Storage& storage) {
    if (cached.worldToPixel) {
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
    const float fov = 5._f * distances[mid];
    cached.worldToPixel = cached.imageSize.y / fov;
    ASSERT(fov > EPS);
}

Optional<ProjectedPoint> OrthoCamera::project(const Vector& r) const {
    const float x = dot(r - cached.position, cached.u) * cached.worldToPixel.value();
    const float y = dot(r - cached.position, cached.v) * cached.worldToPixel.value();
    const Coords point = Coords(0.5f * cached.imageSize.x + x, 0.5f * cached.imageSize.y - y);
    return { { point, float(r[H]) * cached.worldToPixel.value() } };
}

CameraRay OrthoCamera::unproject(const Coords& coords) const {
    const Coords r = (coords - Coords(cached.imageSize * 0.5f)) / cached.worldToPixel.value();
    CameraRay ray;
    ray.origin = cached.position + cached.u * r.x - cached.v * r.y;
    ray.target = ray.origin + cached.w;
    return ray;
}

Vector OrthoCamera::getDirection() const {
    return cached.w;
}

Optional<float> OrthoCamera::getCutoff() const {
    return cached.cutoff;
}

Optional<float> OrthoCamera::getWorldToPixel() const {
    return cached.worldToPixel.value();
}

void OrthoCamera::setCutoff(const Optional<float> newCutoff) {
    cached.cutoff = newCutoff;
}

void OrthoCamera::zoom(const Pixel fixedPoint, const float magnitude) {
    ASSERT(magnitude > 0.f);

    const Vector fixed1 = this->unproject(Coords(fixedPoint)).origin;
    cached.worldToPixel.value() *= magnitude;
    const Vector fixed2 = this->unproject(Coords(fixedPoint)).origin;
    cached.position += fixed1 - fixed2;
}

void OrthoCamera::orbit(const Vector& pivot, const AffineMatrix& matrix) {
    // rotate camera directions
    cached.u = matrix * cached.u;
    cached.v = matrix * cached.v;
    cached.w = cross(cached.u, cached.v);

    // orbit the camera position
    cached.position = pivot + matrix * (cached.position - pivot);
}

void OrthoCamera::reset() {
    cached.w = getNormalized(data.target - data.position);
    cached.v = getNormalized(data.up - dot(data.up, cached.w) * cached.w);
    ASSERT(almostEqual(dot(cached.w, cached.v), 0._f));

    cached.u = cross(cached.v, cached.w);
    ASSERT(getSqrLength(cached.u) == 1._f);
}

void OrthoCamera::pan(const Pixel offset) {
    cached.position -= (cached.u * offset.x + cached.v * offset.y) / cached.worldToPixel.value();
}

void OrthoCamera::resize(const Pixel newSize) {
    cached.imageSize = newSize;
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

PerspectiveCamera::PerspectiveCamera(const CameraData& data)
    : data(data) {
    ASSERT(data.perspective.clipping.lower() > 0._f && data.perspective.clipping.size() > EPS);

    this->reset();
}

void PerspectiveCamera::initialize(const Storage& storage) {
    /// \todo auto-view, like OrthoCamera ?

    /*if (data.tracker) {
        const Vector newTarget = data.tracker->position(storage);
        data.position += newTarget - data.target;
        data.target = newTarget;
        this->update();
    }*/
    (void)storage;
}

Optional<ProjectedPoint> PerspectiveCamera::project(const Vector& r) const {
    const Vector dr = r - data.position;
    const Float proj = dot(dr, cached.dir);
    if (!data.perspective.clipping.contains(proj)) {
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

CameraRay PerspectiveCamera::unproject(const Coords& coords) const {
    const float rx = 2.f * coords.x / data.imageSize.x - 1.f;
    const float ry = 2.f * coords.y / data.imageSize.y - 1.f;
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

Optional<float> PerspectiveCamera::getWorldToPixel() const {
    // it depends on position, so we return NOTHING for simplicity
    return NOTHING;
}

void PerspectiveCamera::setCutoff(const Optional<float> UNUSED(newCutoff)) {}

void PerspectiveCamera::zoom(const Pixel UNUSED(fixedPoint), const float magnitude) {
    ASSERT(magnitude > 0.f);
    data.perspective.fov *= 1._f + 0.01_f * magnitude;
    this->orbit(data.position, cached.matrix);
}

void PerspectiveCamera::orbit(const Vector& UNUSED(pivot), const AffineMatrix& matrix) {
    // reset the previous transform
    // this->reset();

    cached.dir = matrix * cached.dir;
    cached.up = matrix * cached.up;
    cached.left = matrix * cached.left;
    cached.matrix = matrix;
}

void PerspectiveCamera::reset() {
    cached.dir = getNormalized(data.target - data.position);

    // make sure the up vector is perpendicular
    Vector up = data.up;
    up = getNormalized(up - dot(up, cached.dir) * cached.dir);
    ASSERT(abs(dot(up, cached.dir)) < EPS);

    const float aspect = float(data.imageSize.x) / float(data.imageSize.y);
    ASSERT(aspect >= 1.f); // not really required, using for simplicity
    const float tgfov = tan(0.5f * data.perspective.fov);
    cached.up = tgfov / aspect * up;
    cached.left = tgfov * getNormalized(cross(cached.up, cached.dir));
}

void PerspectiveCamera::pan(const Pixel offset) {
    const float x = float(offset.x) / data.imageSize.x;
    const float y = float(offset.y) / data.imageSize.y;
    const Vector worldOffset = getLength(data.target - data.position) * (cached.left * x + cached.up * y);
    data.position -= worldOffset;
    data.target -= worldOffset;
}

void PerspectiveCamera::resize(const Pixel newSize) {
    data.imageSize = newSize;
    this->reset();
}

NAMESPACE_SPH_END
