#include "gui/renderers/VolumeRenderer.h"
#include "gui/Factory.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/renderers/FrameBuffer.h"

NAMESPACE_SPH_BEGIN

inline auto seeder() {
    return [seed = 1337]() mutable { return seed++; };
}

VolumeRenderer::ThreadData::ThreadData(int seed)
    : rng(seed) {}

VolumeRenderer::VolumeRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings)
    : scheduler(scheduler)
    , threadData(*scheduler, seeder()) {
    kernel = CubicSpline<3>();
    fixed.colorMap = Factory::getColorMap(settings);
    fixed.iterationLimit = settings.get<int>(GuiSettingsId::RAYTRACE_ITERATION_LIMIT);
    fixed.emission = settings.get<Float>(GuiSettingsId::VOLUME_EMISSION);
    fixed.enviro.color = settings.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    shouldContinue = true;
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

    shouldContinue = true;
}

void VolumeRenderer::render(const RenderParams& params,
    Statistics& UNUSED(stats),
    IRenderOutput& output) const {
    shouldContinue = true;
    SPH_ASSERT(bvh.getBoundingBox().volume() > 0._f);
    FrameBuffer fb(params.size);
    for (Size iteration = 0; iteration < fixed.iterationLimit && shouldContinue; ++iteration) {
        this->refine(params, iteration, fb);

        const bool isFinal = (iteration == fixed.iterationLimit - 1);
        if (fixed.colorMap) {
            Bitmap<Rgba> bitmap = fixed.colorMap->map(fb.getBitmap());
            output.update(bitmap, {}, isFinal);
        } else {
            output.update(fb.getBitmap(), {}, isFinal);
        }
    }
}

/// \todo deduplicate
INLINE float sampleTent(const float x) {
    if (x < 0.5f) {
        return sqrt(2.f * x) - 1.f;
    } else {
        return 1.f - sqrt(1.f - 2.f * (x - 0.5f));
    }
}

INLINE Coords sampleTent2d(const float halfWidth, UniformRng& rng) {
    const float x = 0.5f + sampleTent(float(rng())) * halfWidth;
    const float y = 0.5f + sampleTent(float(rng())) * halfWidth;
    return Coords(x, y);
}

void VolumeRenderer::refine(const RenderParams& params, const Size iteration, FrameBuffer& fb) const {
    Bitmap<Rgba> bitmap(params.size);

    const bool first = (iteration == 0);
    parallelFor(*scheduler,
        threadData,
        0,
        Size(bitmap.size().y),
        1,
        [this, &bitmap, &params, first](Size y, ThreadData& data) {
            if (!shouldContinue && !first) {
                return;
            }
            for (Size x = 0; x < Size(bitmap.size().x); ++x) {
                const Coords pixel = Coords(x, y) + sampleTent2d(params.surface.filterWidth / 2.f, data.rng);
                const Optional<CameraRay> cameraRay = params.camera->unproject(pixel);
                if (!cameraRay) {
                    bitmap[Pixel(x, y)] = Rgba::black();
                    continue;
                }

                const Vector dir = getNormalized(cameraRay->target - cameraRay->origin);
                const Ray ray(cameraRay->origin, dir);
                bitmap[Pixel(x, y)] = shade(ray, data);
            }
        });

    if (!shouldContinue && !first) {
        return;
    }
    fb.accumulate(bitmap);
}

Rgba VolumeRenderer::shade(const Ray& ray, ThreadData& data) const {
    bvh.getAllIntersections(ray, data.intersections);
    Rgba result = fixed.enviro.color;
    for (const IntersectionInfo& is : data.intersections) {
        const BvhSphere* s = static_cast<const BvhSphere*>(is.object);
        const Size i = s->userData;
        const Vector hit = ray.origin() + ray.direction() * is.t;
        const Vector center = cached.r[i];
        const Vector toCenter = getNormalized(center - hit);
        const Float cosPhi = abs(dot(toCenter, ray.direction()));
        const Float secant = 2._f * getLength(center - hit) * cosPhi;
        result += cached.colors[i] * fixed.emission * secant;
    }
    return result;
}

NAMESPACE_SPH_END
