#include "gui/renderers/ParticleRenderer.h"
#include "gui/Utils.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Color.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/RenderContext.h"
#include "objects/finders/Order.h"
#include "objects/wrappers/Finally.h"
#include "post/Plot.h"
#include "post/Point.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

static void drawVector(IRenderContext& context, const ICamera& camera, const Vector& r, const Vector& v) {
    const Float scale = getLength(v);
    if (scale < EPS) {
        return;
    }
    const Optional<ProjectedPoint> p1 = camera.project(r);
    const Optional<ProjectedPoint> p2 = camera.project(r + v);
    if (!p1 || !p2) {
        return;
    }
    context.setColor(Rgba(1.f, 0.65f, 0.f), ColorFlag::LINE);
    context.setThickness(2.f);
    context.drawLine(p1->coords, p2->coords);

    // make an arrow
    AffineMatrix2 rot = AffineMatrix2::rotate(20._f * DEG_TO_RAD);
    Coords dp = p1->coords - p2->coords;
    PlotPoint dir(dp.x, dp.y);
    PlotPoint a1 = rot.transformPoint(dir) * 0.1f;
    PlotPoint a2 = rot.transpose().transformPoint(dir) * 0.1f;

    context.drawLine(p2->coords, p2->coords + Coords(a1.x, a1.y));
    context.drawLine(p2->coords, p2->coords + Coords(a2.x, a2.y));
}

static void drawPalette(IRenderContext& context, const Rgba& lineColor, const Palette& palette) {
    const int size = 201;
    Pixel origin(context.size().x - 50, size + 30);

    // draw palette
    for (Size i = 0; i < size; ++i) {
        const float value = palette.relativeToPalette(float(i) / (size - 1));
        context.setColor(palette(value), ColorFlag::LINE);
        context.drawLine(Coords(origin.x, origin.y - i), Coords(origin.x + 30, origin.y - i));
    }

    // draw tics
    const Interval interval = palette.getInterval();
    const PaletteScale scale = palette.getScale();

    Array<Float> tics;
    switch (scale) {
    case PaletteScale::LINEAR:
        tics = getLinearTics(interval, 4);
        break;
    case PaletteScale::LOGARITHMIC:
        tics = getLogTics(interval, 4);
        break;
    case PaletteScale::HYBRID: {
        const Size ticsCnt = 5;
        // tics currently not implemented, so just split the range to equidistant steps
        for (Size i = 0; i < ticsCnt; ++i) {
            tics.push(palette.relativeToPalette(float(i) / (ticsCnt - 1)));
        }
        break;
    }
    default:
        NOT_IMPLEMENTED;
    }
    context.setColor(lineColor, ColorFlag::LINE | ColorFlag::TEXT);
    for (Float tic : tics) {
        const Float value = palette.paletteToRelative(tic);
        const Size i = value * size;
        context.drawLine(Coords(origin.x, origin.y - i), Coords(origin.x + 6, origin.y - i));
        context.drawLine(Coords(origin.x + 24, origin.y - i), Coords(origin.x + 30, origin.y - i));

        std::wstring text = toPrintableString(tic, 1, 1000);
        context.drawText(
            Coords(origin.x - 15, origin.y - i), TextAlign::LEFT | TextAlign::VERTICAL_CENTER, text);
    }
}

static void drawGrid(IRenderContext& context, const ICamera& camera, const float grid) {
    // find (any) direction in the camera plane
    const CameraRay originRay = camera.unproject(Coords(0, 0));
    const Vector dir = getNormalized(originRay.target - originRay.origin);
    Vector perpDir;
    if (dir == Vector(0._f, 0._f, 1._f)) {
        perpDir = Vector(1._f, 0._f, 0._f);
    } else {
        perpDir = getNormalized(cross(dir, Vector(0._f, 0._f, 1._f)));
    }

    // find how much is projected grid distance
    const Coords shifted = camera.project(originRay.origin + grid * perpDir)->coords;
    const float dx = getLength(shifted);
    const float dy = dx;
    const Coords origin = camera.project(Vector(0._f))->coords;

    context.setColor(Rgba(0.16_f), ColorFlag::LINE);
    const Pixel size = context.size();
    for (float x = origin.x; x < size.x; x += dx) {
        context.drawLine(Coords(x, 0), Coords(x, size.y));
    }
    for (float x = origin.x - dx; x >= 0; x -= dx) {
        context.drawLine(Coords(x, 0), Coords(x, size.y));
    }
    for (float y = origin.y; y < size.y; y += dy) {
        context.drawLine(Coords(0, y), Coords(size.x, y));
    }
    for (float y = origin.y - dy; y >= 0; y -= dy) {
        context.drawLine(Coords(0, y), Coords(size.x, y));
    }
}

ParticleRenderer::ParticleRenderer(const GuiSettings& settings) {
    cutoff = settings.get<Float>(GuiSettingsId::ORTHO_CUTOFF);
    grid = settings.get<Float>(GuiSettingsId::VIEW_GRID_SIZE);
    background = settings.get<Rgba>(GuiSettingsId::BACKGROUND_COLOR);
    shouldContinue = true;
}

bool ParticleRenderer::isCutOff(const ICamera& camera, const Vector& r) {
    return cutoff != 0._f && abs(dot(camera.getDirection(), r)) > cutoff;
}

void ParticleRenderer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& camera) {
    MEASURE_SCOPE("ParticleRenderer::initialize");
    cached.idxs.clear();
    cached.positions.clear();
    cached.colors.clear();
    cached.vectors.clear();

    bool hasVectorData = false;
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        const Rgba color = colorizer.evalColor(i);
        const Optional<ProjectedPoint> p = camera.project(r[i]);
        if (p && !this->isCutOff(camera, r[i])) {
            cached.idxs.push(i);
            cached.positions.push(r[i]);
            cached.colors.push(color);

            if (Optional<Vector> v = colorizer.evalVector(i)) {
                cached.vectors.push(v.value());
                hasVectorData = true;
            } else {
                cached.vectors.push(Vector(0._f));
            }
        }
    }
    // sort in z-order
    const Vector dir = camera.getDirection();
    Order order(cached.positions.size());
    order.shuffle([this, &dir](Size i, Size j) {
        const Vector r1 = cached.positions[i];
        const Vector r2 = cached.positions[j];
        return dot(dir, r1) > dot(dir, r2);
    });
    cached.positions = order.apply(cached.positions);
    cached.idxs = order.apply(cached.idxs);
    cached.colors = order.apply(cached.colors);

    if (hasVectorData) {
        cached.vectors = order.apply(cached.vectors);
    } else {
        cached.vectors.clear();
    }

    cached.palette = colorizer.getPalette();
}

void ParticleRenderer::render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const {
    MEASURE_SCOPE("ParticleRenderer::render");
    Bitmap<Rgba> bitmap(params.size);
    PreviewRenderContext context(bitmap);

    // draw black background
    context.fill(Rgba::black());

    if (grid > 0.f) {
        drawGrid(context, *params.camera, grid);
    }

    struct {
        Vector r;
        Vector v;
        bool used = false;
    } dir;

    context.setColor(Rgba::black(), ColorFlag::LINE);

    shouldContinue = true;
    // draw particles
    for (Size i = 0; i < cached.positions.size() /* && shouldContinue*/; ++i) {
        if (params.selectedParticle && cached.idxs[i] == params.selectedParticle.value()) {
            // highlight the selected particle
            context.setColor(Rgba::red(), ColorFlag::FILL);
            context.setColor(Rgba::white(), ColorFlag::LINE);

            if (!cached.vectors.empty()) {
                dir.used = true;
                if (params.vectors.toLog) {
                    Float len;
                    tieToTuple(dir.v, len) = getNormalizedWithLength(cached.vectors[i]);
                    dir.v = dir.v * log(1._f + len) * params.vectors.scale;
                } else {
                    dir.v = cached.vectors[i] * params.vectors.scale;
                }
                dir.r = cached.positions[i];
            }
        } else {
            context.setColor(cached.colors[i], ColorFlag::FILL | ColorFlag::LINE);
        }

        const Optional<ProjectedPoint> p = params.camera->project(cached.positions[i]);
        ASSERT(p); // cached values must be visible by the camera
        const Float size = p->radius * params.particles.scale;
        context.drawCircle(p->coords, size);
    }
    // after all particles are drawn, draw the velocity vector over
    if (dir.used) {
        drawVector(context, *params.camera, dir.r, dir.v);
    }

    if (cached.palette) {
        drawPalette(context, background.inverse(), cached.palette.value());
    }

    const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
    context.setColor(background.inverse(), ColorFlag::TEXT);
    context.drawText(
        Coords(0, 0), TextAlign::RIGHT | TextAlign::BOTTOM, "t = " + getFormattedTime(1.e3_f * time));

    output.update(bitmap, context.getLabels());
}

void ParticleRenderer::cancelRender() {
    shouldContinue = false;
}

NAMESPACE_SPH_END
