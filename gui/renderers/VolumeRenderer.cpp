#include "gui/renderers/VolumeRenderer.h"
#include "gui/Factory.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/FrameBuffer.h"
#include "objects/finders/KdTree.h"
#include "objects/utility/OutputIterators.h"

NAMESPACE_SPH_BEGIN

VolumeRenderer::VolumeRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings)
    : IRaytracer(scheduler, settings) {
    fixed.denoise = true;
}

VolumeRenderer::~VolumeRenderer() = default;

const float MAX_DISTENTION = 50;
const float MIN_NEIGHS = 8;

void VolumeRenderer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    cached.colors.resize(cached.r.size());
    for (Size i = 0; i < cached.r.size(); ++i) {
        cached.colors[i] = colorizer.evalColor(i);
    }

    cached.distention.resize(cached.r.size());

    KdTree<KdNode> tree;
    tree.build(*scheduler, cached.r, FinderFlag::SKIP_RANK);

    Array<BvhSphere> spheres(cached.r.size());
    spheres.reserve(cached.r.size());
    ThreadLocal<Array<NeighborRecord>> neighs(*scheduler);
    parallelFor(*scheduler, neighs, 0, cached.r.size(), [&](const Size i, Array<NeighborRecord>& local) {
        const float initialRadius = cached.r[i][H];
        float radius = initialRadius;
        while (radius < MAX_DISTENTION * initialRadius) {
            tree.findAll(cached.r[i], radius, local);
            if (local.size() >= MIN_NEIGHS) {
                break;
            } else {
                radius *= 1.5f;
            }
        }

        BvhSphere s(cached.r[i], radius);
        s.userData = i;
        spheres[i] = s;

        cached.distention[i] = min(radius / initialRadius, MAX_DISTENTION);
    });
    bvh.build(std::move(spheres));

    for (ThreadData& data : threadData) {
        data.data = RayData{};
    }

    shouldContinue = true;
}

bool VolumeRenderer::isInitialized() const {
    return !cached.r.empty();
}

Rgba VolumeRenderer::shade(const RenderParams& params, const CameraRay& cameraRay, ThreadData& data) const {
    const Vector dir = getNormalized(cameraRay.target - cameraRay.origin);
    const Ray ray(cameraRay.origin, dir);

    RayData& rayData(data.data);
    Array<IntersectionInfo>& intersections = rayData.intersections;
    intersections.clear();
    bvh.getAllIntersections(ray, backInserter(intersections));
    if (params.volume.absorption > 0.f) {
        std::sort(intersections.begin(), intersections.end());
    }

    Rgba result = this->getEnviroColor(cameraRay);
    for (const IntersectionInfo& is : reverse(intersections)) {
        const BvhSphere* s = static_cast<const BvhSphere*>(is.object);
        const Size i = s->userData;
        const Vector hit = ray.origin() + ray.direction() * is.t;
        const Vector center = cached.r[i];
        const Vector toCenter = getNormalized(center - hit);
        const float cosPhi = abs(dot(toCenter, ray.direction()));
        const float distention = cached.distention[i];
        const float secant = 2._f * getLength(center - hit) * cosPhi;
        result = result * exp(-params.volume.absorption * secant);
        // 3th power of cosPhi to give more weight to the sphere center,
        // divide by distention^3; distention should not affect the total emission
        const float magnitude = params.volume.emission * pow<3>(cosPhi / distention) * secant;
        result += cached.colors[i] * magnitude;
        result.a() += magnitude;
    }
    result.a() = min(result.a(), 1.f);
    return result;
}

NAMESPACE_SPH_END
