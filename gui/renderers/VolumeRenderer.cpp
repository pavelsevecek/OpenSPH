#include "gui/renderers/VolumeRenderer.h"
#include "gui/Factory.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/FrameBuffer.h"
#include "objects/finders/KdTree.h"
#include "objects/utility/OutputIterators.h"

NAMESPACE_SPH_BEGIN

VolumeRenderer::VolumeRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings)
    : IRaytracer(scheduler, settings) {}

VolumeRenderer::~VolumeRenderer() = default;

const float MAX_DISTENTION = 50;
const float MIN_NEIGHS = 8;

void VolumeRenderer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    cached.r = storage.getValue<Vector>(QuantityId::POSITION).clone();
    this->setColorizer(colorizer);

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

    ArrayView<const Float> m = storage.getValue<Float>(QuantityId::MASS);
    cached.referenceRadii.resize(cached.r.size());
    if (storage.getMaterialCnt() > 0) {
        for (Size matId = 0; matId < storage.getMaterialCnt(); ++matId) {
            MaterialView mat = storage.getMaterial(matId);
            const Float rho = mat->getParams().has(BodySettingsId::DENSITY)
                                  ? mat->getParam<Float>(BodySettingsId::DENSITY)
                                  : 1000._f;
            for (Size i : mat.sequence()) {
                const Float volume = m[i] / rho;
                cached.referenceRadii[i] = root<3>(3._f * volume / (4._f * PI));
            }
        }
    } else {
        // guess the dentity
        const Float rho = 1000._f;
        for (Size i = 0; i < m.size(); ++i) {
            const Float volume = m[i] / rho;
            cached.referenceRadii[i] = root<3>(3._f * volume / (4._f * PI));
        }
    }

    bvh.build(std::move(spheres));

    for (const Attractor& a : storage.getAttractors()) {
        cached.attractors.push(AttractorData{ a.mass, a.position, a.radius });
    }

    for (ThreadData& data : threadData) {
        data.data = RayData{};
    }

    shouldContinue = true;
}

bool VolumeRenderer::isInitialized() const {
    return !cached.r.empty();
}

void VolumeRenderer::setColorizer(const IColorizer& colorizer) {
    cached.colors.resize(cached.r.size());
    for (Size i = 0; i < cached.r.size(); ++i) {
        cached.colors[i] = colorizer.evalColor(i);
    }
}

Rgba VolumeRenderer::shade(const RenderParams& params, const CameraRay& cameraRay, ThreadData& data) const {
    const Vector primaryDir = getNormalized(cameraRay.target - cameraRay.origin);
    const Ray primaryRay(cameraRay.origin, primaryDir);

    RayData& rayData(data.data);
    Rgba result = this->getEnviroColor(cameraRay);
    HyperbolicRay& curvedRay = rayData.curvedRay;
    curvedRay =
        HyperbolicRay::getFromRay(primaryRay, cached.attractors, params.distortionMagnitude, 3.e5_f, 3.e6_f);

    for (const RaySegment& ray : curvedRay.getSegments()) {
        Array<IntersectionInfo>& intersections = rayData.intersections;
        intersections.clear();
        bvh.getAllIntersections(ray, backInserter(intersections));
        if (params.volume.absorption > 0.f) {
            std::sort(intersections.begin(), intersections.end());
        }

        for (const IntersectionInfo& is : reverse(intersections)) {
            const BvhSphere* s = static_cast<const BvhSphere*>(is.object);
            const Size i = s->userData;
            const Vector hit = ray.origin() + ray.direction() * is.t;
            const Vector center = cached.r[i];
            const Vector toCenter = getNormalized(center - hit);
            const float cosPhi = abs(dot(toCenter, ray.direction()));
            const float distention = cached.distention[i];
            // smoothing length should not have effect on the total emission
            const float radiiFactor = cached.referenceRadii[i] / cached.r[i][H];
            const float secant = 2._f * getLength(center - hit) * cosPhi * radiiFactor;
            // make dilated particles absorb more
            result = result * exp(-params.volume.absorption * secant * distention * pow<3>(cosPhi));
            // 3th power of cosPhi to give more weight to the sphere center,
            // divide by distention^3; distention should not affect the total emission
            const float magnitude = params.volume.emission * pow<3>(cosPhi / distention) * secant;
            result += cached.colors[i] * magnitude;
            result.a() += magnitude;
        }
    }
    result.a() = min(result.a(), 1.f);
    return result;
}

NAMESPACE_SPH_END
