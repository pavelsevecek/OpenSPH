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

SurfaceRenderer::SurfaceRenderer(const GuiSettings& settings) {
    surfaceLevel = settings.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    surfaceResolution = settings.get<Float>(GuiSettingsId::SURFACE_RESOLUTION);
    sunPosition = settings.get<Vector>(GuiSettingsId::SURFACE_SUN_POSITION);
    sunIntensity = settings.get<Float>(GuiSettingsId::SURFACE_SUN_INTENSITY);
    ambient = settings.get<Float>(GuiSettingsId::SURFACE_AMBIENT);
}

void SurfaceRenderer::initialize(const Storage& storage,
    const IColorizer& UNUSED(colorizer),
    const ICamera& UNUSED(camera)) {
    cached.colors.clear();
    cached.triangles = getSurfaceMesh(storage, surfaceResolution, surfaceLevel);

    for (Triangle& t : cached.triangles) {

        // supersimple diffuse shading
        /// \todo color using IColorizer; it is non-trivial to use, since we have no connection of generated
        /// triangles with the particles. This needs to implemented in Marching cubes; we need to compute also
        /// interpolated values of given quantity (beside the surface field).
        float gray = ambient + sunIntensity * max(0._f, dot(sunPosition, t.normal()));

        cached.colors.push(Color(gray));
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
