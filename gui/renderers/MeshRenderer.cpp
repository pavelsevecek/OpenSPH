#include "gui/renderers/MeshRenderer.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Color.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/RenderContext.h"
#include "objects/finders/Order.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"

NAMESPACE_SPH_BEGIN

MeshRenderer::MeshRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& gui)
    : scheduler(scheduler) {
    surfaceLevel = gui.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    surfaceResolution = gui.get<Float>(GuiSettingsId::SURFACE_RESOLUTION);
    sunPosition = gui.get<Vector>(GuiSettingsId::SURFACE_SUN_POSITION);
    sunIntensity = gui.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY);
    ambient = gui.get<Float>(GuiSettingsId::SURFACE_AMBIENT);

    RunSettings settings;
    settings.set(RunSettingsId::SPH_FINDER, FinderEnum::KD_TREE);
    finder = Factory::getFinder(settings);
    kernel = Factory::getKernel<3>(settings);
}

void MeshRenderer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    cached.colors.clear();

    const Box boundingBox = getBoundingBox(storage);
    const Float dim = maxElement(boundingBox.size());
    // clamp to avoid extreme resolution (which would most likely cause bad alloc)
    const Float actResolution = clamp(surfaceResolution, 0.001_f * dim, 0.1_f * dim);

    // get the surface as triangles
    cached.triangles = getSurfaceMesh(*scheduler, storage, actResolution, surfaceLevel, 1._f);

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    finder->build(*scheduler, r);
    Array<NeighbourRecord> neighs;

    for (Triangle& t : cached.triangles) {
        const Vector pos = t.center();
        finder->findAll(pos, 4._f * actResolution, neighs);

        Rgba colorSum(0._f);
        Float weightSum = 0._f;
        for (auto& n : neighs) {
            const Size i = n.index;
            const Rgba color = colorizer.evalColor(i);
            /// \todo fix, the weight here should be consistent with MC
            const Float w = max(kernel.value(r[i] - pos, r[i][H]), EPS);
            colorSum += color * w;
            weightSum += w;
        }

        if (weightSum == 0._f) {
            // we somehow didn't find any neighbours, indicate the error by red triangle
            cached.colors.push(Rgba::red());
        } else {
            // supersimple diffuse shading
            const float gray = ambient + sunIntensity * max(0._f, dot(sunPosition, t.normal()));
            cached.colors.push(colorSum / weightSum * gray);
        }
    }
}

void MeshRenderer::render(const RenderParams& params, Statistics& stats, IRenderOutput& output) const {
    Bitmap<Rgba> bitmap(params.size);
    PreviewRenderContext context(bitmap);

    // draw black background
    context.fill(Rgba::black());

    // sort the arrays by z-depth
    Order triangleOrder(cached.triangles.size());
    const Vector cameraDir = params.camera->getDirection();
    triangleOrder.shuffle([this, &cameraDir](const Size i1, const Size i2) {
        const Vector v1 = cached.triangles[i1].center();
        const Vector v2 = cached.triangles[i2].center();
        return dot(cameraDir, v1) > dot(cameraDir, v2);
    });

    // draw all triangles, starting from the ones with largest z-depth
    for (Size i = 0; i < cached.triangles.size(); ++i) {
        const Size idx = triangleOrder[i];
        context.setColor(cached.colors[idx], ColorFlag::LINE | ColorFlag::FILL);

        Optional<ProjectedPoint> p1 = params.camera->project(cached.triangles[idx][0]);
        Optional<ProjectedPoint> p2 = params.camera->project(cached.triangles[idx][1]);
        Optional<ProjectedPoint> p3 = params.camera->project(cached.triangles[idx][2]);
        if (!p1 || !p2 || !p3) {
            continue;
        }
        context.drawTriangle(p1->coords, p2->coords, p3->coords);
    }

    if (stats.has(StatisticsId::RUN_TIME)) {
        const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
        context.drawText(Coords(0, 0), TextAlign::RIGHT | TextAlign::BOTTOM, getFormattedTime(time * 1000));
    }

    output.update(bitmap, context.getLabels(), true);
}

NAMESPACE_SPH_END
