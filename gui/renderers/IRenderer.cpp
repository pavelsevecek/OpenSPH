#include "gui/renderers/IRenderer.h"
#include "gui/Factory.h"
#include "gui/ImageTransform.h"
#include "gui/Settings.h"
#include "gui/Utils.h"
#include "gui/objects/Camera.h"
#include "gui/objects/RenderContext.h"
#include "gui/renderers/FrameBuffer.h"
#include "system/Profiler.h"

NAMESPACE_SPH_BEGIN

void RenderParams::initialize(const GuiSettings& gui) {
    background = gui.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    showKey = gui.get<bool>(GuiSettingsId::SHOW_KEY);
    particles.scale = float(gui.get<Float>(GuiSettingsId::PARTICLE_RADIUS));
    particles.grayScale = gui.get<bool>(GuiSettingsId::FORCE_GRAYSCALE);
    particles.doAntialiasing = gui.get<bool>(GuiSettingsId::ANTIALIASED);
    particles.smoothed = gui.get<bool>(GuiSettingsId::SMOOTH_PARTICLES);
    particles.renderGhosts = gui.get<bool>(GuiSettingsId::RENDER_GHOST_PARTICLES);
    surface.level = float(gui.get<Float>(GuiSettingsId::SURFACE_LEVEL));
    surface.emission = float(gui.get<Float>(GuiSettingsId::SURFACE_EMISSION));
    surface.filterWidth = float(gui.get<Float>(GuiSettingsId::SURFACE_FILTER_WIDTH));
    volume.emission = float(gui.get<Float>(GuiSettingsId::VOLUME_EMISSION));
    volume.absorption = float(gui.get<Float>(GuiSettingsId::VOLUME_ABSORPTION));
    volume.scattering = float(gui.get<Float>(GuiSettingsId::VOLUME_SCATTERING));
    lighting.ambientLight = float(gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT));
    lighting.sunLight = float(gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY));
    lighting.dirToSun = getNormalized(gui.get<Vector>(GuiSettingsId::SURFACE_SUN_POSITION));
    relativity.lensingMagnitude = gui.get<Float>(GuiSettingsId::RAYTRACE_LENSING_MAGNITUDE);
    relativity.rayStep = gui.get<Float>(GuiSettingsId::RAYTRACE_LENSING_STEP);
    post.compressionFactor = float(gui.get<Float>(GuiSettingsId::COLORMAP_LOGARITHMIC_FACTOR));
    post.denoise = gui.get<bool>(GuiSettingsId::REDUCE_LOWFREQUENCY_NOISE);
    post.bloomRadius = gui.get<Float>(GuiSettingsId::BLOOM_RADIUS);
    post.bloomIntensity = gui.get<Float>(GuiSettingsId::BLOOM_INTENSITY);
}

void renderOverlay(IRenderContext& context, const RenderParams& params, const Statistics& stats) {
    if (params.showKey) {
        if (Optional<float> wtp = params.camera->getWorldToPixel(params.camera->getTarget())) {
            drawKey(context, stats, wtp.value(), params.background);
        }

        const AffineMatrix frame = params.camera->getFrame().inverse();
        drawAxis(context, Rgba::red(), frame.row(0), "x");
        drawAxis(context, Rgba::green(), -frame.row(1), "y");
        drawAxis(context, Rgba::blue(), frame.row(2), "z");
    }
}

inline auto seeder() {
    return [seed = 1337]() mutable { return seed++; };
}

IRaytracer::ThreadData::ThreadData(const int seed)
    : rng(seed) {}

IRaytracer::IRaytracer(SharedPtr<IScheduler> scheduler, const GuiSettings& settings)
    : scheduler(scheduler)
    , threadData(*scheduler, seeder()) {
    fixed.colorMap = Factory::getColorMap(settings);
    fixed.subsampling = settings.get<int>(GuiSettingsId::RAYTRACE_SUBSAMPLING);
    fixed.iterationLimit = settings.get<int>(GuiSettingsId::RAYTRACE_ITERATION_LIMIT);

    fixed.enviro.color = settings.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    String hdriPath = settings.get<String>(GuiSettingsId::RAYTRACE_HDRI);

    if (!hdriPath.empty()) {
        try {
            fixed.enviro.hdri = Texture(Path(hdriPath), TextureFiltering::BILINEAR);
        } catch (const IoError&) {
            // ignoring for now
        }
    }

    shouldContinue = true;
}

void IRaytracer::render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const {
    shouldContinue = true;

    if (RawPtr<LogarithmicColorMap> logMap = dynamicCast<LogarithmicColorMap>(fixed.colorMap.get())) {
        logMap->setFactor(params.post.compressionFactor);
    }

    FrameBuffer fb(params.camera->getSize());
    for (Size iteration = 0; iteration < fixed.iterationLimit && shouldContinue; ++iteration) {
        this->refine(params, iteration, fb);

        const bool isFinal = (iteration == fixed.iterationLimit - 1);
        postProcess(fb, params, stats, isFinal, output);
    }
}

void IRaytracer::postProcess(FrameBuffer& fb,
    const RenderParams& params,
    const Statistics& stats,
    const bool isFinal,
    IRenderOutput& output) const {
    if (!fixed.colorMap && !params.showKey &&
        (!isFinal || (!params.post.denoise && params.post.bloomIntensity == 0.f))) {
        // no postprocessing in this case, we can optimize and return the bitmap directly
        output.update(fb.getBitmap(), {}, isFinal);
        return;
    }

    Bitmap<Rgba> bitmap;
    if (isFinal) {
        bitmap = std::move(fb).getBitmap();
    } else {
        bitmap = fb.getBitmap().clone();
    }

    if (isFinal && params.post.bloomIntensity > 0.f) {
        const int maxRadius = min(bitmap.size().x, bitmap.size().y) / 2 - 1;
        const int radius = clamp<int>(params.post.bloomRadius * bitmap.size().x, 1, maxRadius);
        bitmap = bloomEffect(*scheduler, bitmap, radius, params.post.bloomIntensity);
    }

    if (fixed.colorMap) {
        fixed.colorMap->map(*scheduler, bitmap);
    }

    if (isFinal && params.post.denoise) {
        bitmap = denoiseLowFrequency(*scheduler, bitmap, {});
    }

    PreviewRenderContext<OverridePixelOp> context(bitmap);
    renderOverlay(context, params, stats);
    output.update(std::move(bitmap), context.getLabels(), isFinal);
}

INLINE float sampleTent(const float x) {
    if (x < 0.5f) {
        return sqrt(2.f * x) - 1.f;
    } else {
        return 1.f - sqrt(1.f - 2.f * (x - 0.5f));
    }
}

INLINE Coords sampleTent2d(const Size level, const float halfWidth, UniformRng& rng) {
    if (level == 1) {
        const float x = 0.5f + sampleTent(float(rng())) * halfWidth;
        const float y = 0.5f + sampleTent(float(rng())) * halfWidth;
        return Coords(x, y);
    } else {
        // center of the pixel
        return Coords(0.5f, 0.5f);
    }
}

void IRaytracer::refine(const RenderParams& params, const Size iteration, FrameBuffer& fb) const {
    MEASURE_SCOPE("Rendering frame");
    const Size level = 1 << max(int(fixed.subsampling) - int(iteration), 0);
    const Pixel size = params.camera->getSize();
    Pixel actSize;
    actSize.x = size.x / level + sgn(size.x % level);
    actSize.y = size.y / level + sgn(size.y % level);
    Bitmap<Rgba> bitmap(actSize);

    const bool first = (iteration == 0);
    parallelFor(*scheduler,
        threadData,
        0,
        Size(bitmap.size().y),
        1,
        [this, &bitmap, &params, level, first](Size y, ThreadData& data) {
            if (!shouldContinue && !first) {
                return;
            }
            for (Size x = 0; x < Size(bitmap.size().x); ++x) {
                const Coords pixel = Coords(x * level, y * level) +
                                     sampleTent2d(level, params.surface.filterWidth / 2.f, data.rng);
                const Optional<CameraRay> cameraRay = params.camera->unproject(pixel);
                if (!cameraRay) {
                    bitmap[Pixel(x, y)] = Rgba::black();
                    continue;
                }

                bitmap[Pixel(x, y)] = this->shade(params, cameraRay.value(), data);
            }
        });

    if (!shouldContinue && !first) {
        return;
    }
    if (level == 1) {
        fb.accumulate(*scheduler, bitmap);
    } else {
        Bitmap<Rgba> full(size);
        for (Size y = 0; y < Size(full.size().y); ++y) {
            for (Size x = 0; x < Size(full.size().x); ++x) {
                full[Pixel(x, y)] = bitmap[Pixel(x / level, y / level)];
            }
        }
        fb.override(std::move(full));
    }
}

Rgba IRaytracer::getEnviroColor(const CameraRay& ray) const {
    if (fixed.enviro.hdri.empty()) {
        return fixed.enviro.color;
    } else {
        const Vector dir = ray.target - ray.origin;
        /// \todo deduplicate with setupUvws
        const SphericalCoords spherical = cartensianToSpherical(Vector(dir[X], dir[Z], dir[Y]));
        const Vector uvw(spherical.phi / (2._f * PI) + 0.5_f, spherical.theta / PI, 0._f);
        return fixed.enviro.hdri.eval(uvw);
    }
}


NAMESPACE_SPH_END
