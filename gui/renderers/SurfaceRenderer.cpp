#include "gui/renderers/SurfaceRenderer.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Color.h"
#include "gui/objects/Element.h"
#include "post/MarchingCubes.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <wx/dcmemory.h>

NAMESPACE_SPH_BEGIN

SurfaceRenderer::SurfaceRenderer(const GuiSettings& settings) {
    surfaceLevel = settings.get<Float>(GuiSettingsId::SURFACE_LEVEL);
    surfaceResolution = settings.get<Float>(GuiSettingsId::SURFACE_RESOLUTION);
}

void SurfaceRenderer::initialize(const Storage& storage,
    const Abstract::Element& UNUSED(element),
    const Abstract::Camera& camera) {
    cached.colors.clear();
    cached.triangles.clear();

    /// \todo get these values from GuiSettings!
    Array<Triangle> triangles = getSurfaceMesh(storage, surfaceResolution, surfaceLevel);

    const Vector n(0._f, 0._f, 1._f);
    for (Triangle& t : triangles) {
        const Float cosPhi = dot(n, t.normal());
        if (cosPhi < 0._f) {
            // facing the other way
            continue;
        }
        // supersimple diffuse shading
        float gray = 0.4f + 0.5 * cosPhi;

        StaticArray<Optional<ProjectedPoint>, 3> ps;
        for (Size i = 0; i < 3; ++i) {
            t[i][H] = 1._f; // something reasonable
            ps[i] = camera.project(t[i]);
        }
        if (ps[0] && ps[1] && ps[2]) {
            cached.triangles.push(ps[0]->point);
            cached.triangles.push(ps[1]->point);
            cached.triangles.push(ps[2]->point);
            cached.colors.push(Color(gray));
        }
    }
}

SharedPtr<Bitmap> SurfaceRenderer::render(const Abstract::Camera& UNUSED(camera),
    const RenderParams& params,
    Statistics& stats) const {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    MEASURE_SCOPE("SurfaceRenderer::render");
    const wxSize size(params.size.x, params.size.y);
    wxBitmap bitmap(size, 24);
    wxMemoryDC dc(bitmap);

    // draw black background (there is no fill method?)
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(wxPoint(0, 0), size);

    // draw particles
    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);

    ASSERT(cached.triangles.size() % 3 == 0);
    for (Size i = 0; i < cached.triangles.size() / 3; ++i) {
        brush.SetColour(wxColour(cached.colors[i]));
        pen.SetColour(wxColour(cached.colors[i]));
        dc.SetBrush(brush);
        dc.SetPen(pen);

        wxPoint pts[3];
        pts[0] = cached.triangles[3 * i + 0];
        pts[1] = cached.triangles[3 * i + 1];
        pts[2] = cached.triangles[3 * i + 2];
        dc.DrawPolygon(3, pts);
    }
    const Float time = stats.get<Float>(StatisticsId::TOTAL_TIME);
    dc.DrawText(("t = " + std::to_string(time) + "s").c_str(), wxPoint(0, 0));

    dc.SelectObject(wxNullBitmap);
    return makeShared<Bitmap>(std::move(bitmap));
}

NAMESPACE_SPH_END
