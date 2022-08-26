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
#include "quantities/Attractor.h"
#include "sph/boundary/Boundary.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

static void drawVector(IRenderContext& context,
    const ICamera& camera,
    const Vector& r,
    const Vector& v,
    const Float length) {
    if (getSqrLength(v) == 0._f) {
        return;
    }
    const Optional<ProjectedPoint> p1 = camera.project(r);
    const Optional<ProjectedPoint> p2 = camera.project(r + v);
    if (!p1 || !p2) {
        return;
    }

    Coords dir = p2->coords - p1->coords;
    const Float l = getLength(dir);
    if (l == 0._f) {
        return;
    }
    dir *= float(length / l);
    const Coords c1 = p1->coords;
    const Coords c2 = p1->coords + dir;

    context.setColor(Rgba(1.f, 0.65f, 0.f), ColorFlag::LINE);
    context.setThickness(2.f);
    context.drawLine(c1, c2);

    // make an arrow
    AffineMatrix2 rot = AffineMatrix2::rotate(160._f * DEG_TO_RAD);
    PlotPoint dp(dir.x, dir.y);
    PlotPoint a1 = rot.transformPoint(dp) * 0.1f;
    PlotPoint a2 = rot.transpose().transformPoint(dp) * 0.1f;

    context.drawLine(c2, c2 + Coords(float(a1.x), float(a1.y)));
    context.drawLine(c2, c2 + Coords(float(a2.x), float(a2.y)));
}

static void drawGrid(IRenderContext& context, const ICamera& camera, const float grid) {
    // find (any) direction in the camera plane
    const Optional<CameraRay> originRay = camera.unproject(Coords(0, 0));
    const Vector dir = getNormalized(originRay->target - originRay->origin);
    Vector perpDir;
    if (dir == Vector(0._f, 0._f, 1._f)) {
        perpDir = Vector(1._f, 0._f, 0._f);
    } else {
        perpDir = getNormalized(cross(dir, Vector(0._f, 0._f, 1._f)));
    }

    // find how much is projected grid distance
    const Coords shifted = camera.project(originRay->origin + grid * perpDir)->coords;
    const float dx = getLength(shifted);
    const float dy = dx;
    const Coords origin = camera.project(Vector(0._f))->coords;

    context.setColor(Rgba(0.16f), ColorFlag::LINE);
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
    grid = float(settings.get<Float>(GuiSettingsId::VIEW_GRID_SIZE));
    shouldContinue = true;
}

static bool isCutOff(const Vector& r, const Optional<float> cutoff, const Vector direction) {
    return cutoff && abs(dot(direction, r)) > cutoff.value();
}

constexpr Size GHOST_INDEX = Size(-1);
constexpr Size ATTRACTOR_INDEX = Size(-2);

void ParticleRenderer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& camera) {
    MEASURE_SCOPE("ParticleRenderer::initialize");
    cached.idxs.clear();
    cached.positions.clear();
    cached.colors.clear();
    cached.vectors.clear();

    const Optional<float> cutoff = camera.getCutoff();
    const Vector direction = camera.getFrame().row(2);
    bool hasVectorData = bool(colorizer.evalVector(0));
    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    for (Size i = 0; i < r.size(); ++i) {
        const Optional<ProjectedPoint> p = camera.project(r[i]);
        if (p && !isCutOff(r[i], cutoff, direction)) {
            cached.idxs.push(i);
            cached.positions.push(r[i]);

            const Rgba color = colorizer.evalColor(i);
            cached.colors.push(color);

            if (hasVectorData) {
                Optional<Vector> v = colorizer.evalVector(i);
                SPH_ASSERT(v);
                cached.vectors.push(v.value());
            }
        }
    }

    SharedPtr<IStorageUserData> data = storage.getUserData();
    if (RawPtr<GhostParticlesData> ghosts = dynamicCast<GhostParticlesData>(data.get())) {
        for (Size i = 0; i < ghosts->size(); ++i) {
            const Vector pos = ghosts->getGhost(i).position;
            if (!isCutOff(pos, cutoff, direction)) {
                cached.idxs.push(GHOST_INDEX);
                cached.positions.push(pos);
                cached.colors.push(Rgba::transparent());

                if (hasVectorData) {
                    cached.vectors.push(Vector(0._f));
                }
            }
        }
    }

    for (const Attractor& a : storage.getAttractors()) {
        if (!isCutOff(a.position, cutoff, direction)) {
            cached.idxs.push(ATTRACTOR_INDEX);
            cached.positions.push(setH(a.position, a.radius));
            cached.colors.push(Rgba::white());

            if (hasVectorData) {
                cached.vectors.push(Vector(0._f));
            }
        }
    }

    // sort in z-order
    Order order(cached.positions.size());
    order.shuffle([this, &direction](Size i, Size j) {
        const Vector r1 = cached.positions[i];
        const Vector r2 = cached.positions[j];
        return dot(direction, r1) > dot(direction, r2);
    });
    /// \todo could be changed to AOS to sort only once
    cached.positions = order.apply(cached.positions);
    cached.idxs = order.apply(cached.idxs);
    cached.colors = order.apply(cached.colors);

    cached.cameraDir = direction;

    if (hasVectorData) {
        cached.vectors = order.apply(cached.vectors);
    } else {
        cached.vectors.clear();
    }
}

bool ParticleRenderer::isInitialized() const {
    return !cached.positions.empty();
}

void ParticleRenderer::setColorizer(const IColorizer& colorizer) {
    for (Size i = 0; i < cached.idxs.size(); ++i) {
        if (cached.idxs[i] == GHOST_INDEX || cached.idxs[i] == ATTRACTOR_INDEX) {
            continue; // ghost or attractor
        }
        cached.colors[i] = colorizer.evalColor(cached.idxs[i]);
    }
}

static AutoPtr<IRenderContext> getContext(const RenderParams& params, Bitmap<Rgba>& bitmap) {
    if (params.particles.doAntialiasing) {
        if (params.particles.smoothed) {
            CubicSpline<2> kernel;
            return makeAuto<SmoothedRenderContext>(bitmap, kernel);
        } else {
            return makeAuto<AntiAliasedRenderContext>(bitmap);
        }
    } else {
        if (params.background.a() == 1.f) {
            return makeAuto<PreviewRenderContext<OverridePixelOp>>(bitmap);
        } else {
            return makeAuto<PreviewRenderContext<OverPixelOp>>(bitmap);
        }
    }
}

void ParticleRenderer::render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const {
    MEASURE_SCOPE("ParticleRenderer::render");

    Bitmap<Rgba> bitmap(params.camera->getSize());
    AutoPtr<IRenderContext> context = getContext(params, bitmap);

    // fill with the background color
    context->fill(params.background);

    if (grid > 0.f) {
        drawGrid(*context, *params.camera, grid);
    }

    struct {
        Vector r;
        Vector v;
        bool used = false;
    } dir;

    context->setColor(Rgba::black(), ColorFlag::LINE);

    shouldContinue = true;
    // draw particles
    const bool reverseOrder = dot(cached.cameraDir, params.camera->getFrame().row(2)) < 0._f;
    for (Size k = 0; k < cached.positions.size(); ++k) {
        const Size i = reverseOrder ? cached.positions.size() - k - 1 : k;
        if (!params.particles.renderGhosts && cached.idxs[i] == GHOST_INDEX) {
            continue;
        }
        if (params.particles.selected && cached.idxs[i] == params.particles.selected.value()) {
            // highlight the selected particle
            context->setColor(Rgba::red(), ColorFlag::FILL);
            context->setColor(Rgba::white(), ColorFlag::LINE);

            if (!cached.vectors.empty()) {
                dir.used = true;
                dir.v = cached.vectors[i];
                dir.r = cached.positions[i];
            }
        } else {
            Rgba color = cached.colors[i];
            if (params.particles.grayScale) {
                color = Rgba(color.intensity());
            }
            context->setColor(color, ColorFlag::FILL | ColorFlag::LINE);
            if (cached.idxs[i] == GHOST_INDEX || cached.idxs[i] == ATTRACTOR_INDEX) {
                context->setColor(Rgba::gray(0.7f), ColorFlag::LINE);
            }
        }

        const Optional<ProjectedPoint> p = params.camera->project(cached.positions[i]);
        if (p) {
            float radius;
            if (cached.idxs[i] != ATTRACTOR_INDEX) {
                radius = p->radius * params.particles.scale;
            } else {
                radius = p->radius;
            }
            const float size = min<float>(radius, context->size().x);
            context->drawCircle(p->coords, size);
        }
    }
    // after all particles are drawn, draw the velocity vector over
    if (dir.used) {
        drawVector(*context, *params.camera, dir.r, dir.v, params.vectors.length);
    }

    renderOverlay(*context, params, stats);

    // lastly black frame to draw on top of other stuff
    const Pixel upper = bitmap.size() - Pixel(1, 1);
    context->setColor(Rgba::black(), ColorFlag::LINE);
    context->drawLine(Coords(0, 0), Coords(upper.x, 0));
    context->drawLine(Coords(upper.x, 0), Coords(upper));
    context->drawLine(Coords(upper), Coords(0, upper.y));
    context->drawLine(Coords(0, upper.y), Coords(0, 0));

    output.update(bitmap, context->getLabels(), true);
}

void ParticleRenderer::cancelRender() {
    shouldContinue = false;
}

NAMESPACE_SPH_END
