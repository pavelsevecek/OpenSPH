#include "gui/renderers/VolumeRenderer.h"
#include "gui/Factory.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/FrameBuffer.h"

NAMESPACE_SPH_BEGIN

VolumeRenderer::VolumeRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings)
    : IRaytracer(scheduler, settings) {
    kernel = CubicSpline<3>();
}

VolumeRenderer::~VolumeRenderer() = default;

void VolumeRenderer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    cached.colors.resize(cached.r.size());
    for (Size i = 0; i < cached.r.size(); ++i) {
        cached.colors[i] = colorizer.evalColor(i);
    }

    Array<BvhSphere> spheres;
    spheres.reserve(cached.r.size());
    for (Size i = 0; i < cached.r.size(); ++i) {
        BvhSphere& s = spheres.emplaceBack(cached.r[i], 2.f * cached.r[i][H]);
        s.userData = i;
    }
    bvh.build(std::move(spheres));

    for (ThreadData& data : threadData) {
        data.data = std::set<IntersectionInfo>{};
    }

    shouldContinue = true;
}

Rgba VolumeRenderer::shade(const RenderParams& params, const CameraRay& cameraRay, ThreadData& data) const {
    const Vector dir = getNormalized(cameraRay.target - cameraRay.origin);
    const Ray ray(cameraRay.origin, dir);

    std::set<IntersectionInfo>& intersections(data.data);
    bvh.getAllIntersections(ray, intersections);
    Rgba result = this->getEnviroColor(cameraRay);
    for (const IntersectionInfo& is : intersections) {
        const BvhSphere* s = static_cast<const BvhSphere*>(is.object);
        const Size i = s->userData;
        const Vector hit = ray.origin() + ray.direction() * is.t;
        const Vector center = cached.r[i];
        const Vector toCenter = getNormalized(center - hit);
        const Float cosPhi = abs(dot(toCenter, ray.direction()));
        // 4th power to give more weight to the sphere center
        const Float secant = 2._f * getLength(center - hit) * pow<4>(cosPhi);
        result += cached.colors[i] * params.volume.emission * secant;
    }
    return result;
}

NAMESPACE_SPH_END
