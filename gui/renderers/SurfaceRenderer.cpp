#include "gui/renderers/SurfaceRenderer.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Color.h"
#include "gui/objects/Colorizer.h"
#include "objects/finders/Order.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <wx/dcmemory.h>

NAMESPACE_SPH_BEGIN

SurfaceRenderer::SurfaceRenderer(const GuiSettings& gui) {
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

void SurfaceRenderer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    cached.colors.clear();

    // get the surface as triangles
    cached.triangles = getSurfaceMesh(storage, surfaceResolution, surfaceLevel);

    ArrayView<const Vector> r = storage.getValue<Vector>(QuantityId::POSITION);
    /*Float maxH = 0._f;
    for (Size i = 0; i < r.size(); ++i) {
        maxH = max(maxH, r[i][H]);
    }*/

    finder->build(r);
    Array<NeighbourRecord> neighs;

    for (Triangle& t : cached.triangles) {
        const Vector pos = t.center();
        /// \todo test wxGraphicsContext::CreateLinearGradientBrush, it might be possible to interpolate
        /// colors between triangle vertices
        finder->findAll(pos, 2.f * surfaceResolution, neighs);

        Color colorSum(0._f);
        Float weightSum = 0._f;
        for (auto& n : neighs) {
            const Size i = n.index;
            const Color color = colorizer.evalColor(i);
            const Float w = kernel.value(r[i] - pos, surfaceResolution);
            colorSum += color * w;
            weightSum += w;
        }

        if (weightSum == 0._f) {
            // we somehow didn't find any neighbours, indicate the error by red triangle
            cached.colors.push(Color::red());
        } else {
            // supersimple diffuse shading
            const float gray = ambient + sunIntensity * max(0._f, dot(sunPosition, t.normal()));
            cached.colors.push(colorSum / weightSum * gray);
        }
    }
}

SharedPtr<Bitmap> SurfaceRenderer::render(const ICamera& camera,
    const RenderParams& params,
    Statistics& stats) const {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    // MEASURE_SCOPE("SurfaceRenderer::render");
    const wxSize size(params.size.x, params.size.y);
    wxBitmap bitmap(size, 24);
    wxMemoryDC dc(bitmap);

    // draw black background (there is no fill method?)
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(wxPoint(0, 0), size);

    // draw particles
    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);

    // sort the arrays by z-depth
    Order triangleOrder(cached.triangles.size());
    triangleOrder.shuffle([this, &camera](const Size i1, const Size i2) {
        const Vector v1 = cached.triangles[i1].center();
        const Vector v2 = cached.triangles[i2].center();
        const Vector cameraDir = camera.getDirection();
        return dot(cameraDir, v1) > dot(cameraDir, v2);
    });

    // draw all triangles, starting from the ones with largest z-depth
    for (Size i = 0; i < cached.triangles.size(); ++i) {
        const Size idx = triangleOrder[i];
        brush.SetColour(wxColour(cached.colors[idx]));
        pen.SetColour(wxColour(cached.colors[idx]));
        dc.SetBrush(brush);
        dc.SetPen(pen);

        Optional<ProjectedPoint> p1 = camera.project(cached.triangles[idx][0]);
        Optional<ProjectedPoint> p2 = camera.project(cached.triangles[idx][1]);
        Optional<ProjectedPoint> p3 = camera.project(cached.triangles[idx][2]);
        if (!p1 || !p2 || !p3) {
            continue;
        }
        StaticArray<wxPoint, 3> pts;
        pts[0] = p1->point;
        pts[1] = p2->point;
        pts[2] = p3->point;
        dc.DrawPolygon(3, &pts[0]);
    }

    const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
    dc.DrawText(("t = " + std::to_string(time) + "s").c_str(), wxPoint(0, 0));

    dc.SelectObject(wxNullBitmap);
    return makeShared<Bitmap>(std::move(bitmap));
}

NAMESPACE_SPH_END
