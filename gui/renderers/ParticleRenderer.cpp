#include "gui/renderers/ParticleRenderer.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Color.h"
#include "gui/objects/Colorizer.h"
#include "objects/finders/Order.h"
#include "post/Point.h"
#include "system/Profiler.h"
#include "system/Statistics.h"
#include "thread/CheckFunction.h"
#include <wx/dcmemory.h>

NAMESPACE_SPH_BEGIN


static void drawVector(wxDC& dc, const ICamera& camera, const Vector& r, const Vector& v) {
    const Float scale = getLength(v);
    if (scale < EPS) {
        return;
    }
    const Optional<ProjectedPoint> p1 = camera.project(r);
    const Optional<ProjectedPoint> p2 = camera.project(r + v);
    if (!p1 || !p2) {
        return;
    }
    wxPen pen = dc.GetPen();
    pen.SetColour(wxColour(255, 165, 0));
    pen.SetWidth(2);
    dc.SetPen(pen);
    dc.DrawLine(p1->point, p2->point);

    // make an arrow
    AffineMatrix2 rot = AffineMatrix2::rotate(20._f * DEG_TO_RAD);
    Point dp = p1->point - p2->point;
    PlotPoint dir(dp.x, dp.y);
    PlotPoint a1 = rot.transformPoint(dir) * 0.1f;
    PlotPoint a2 = rot.transpose().transformPoint(dir) * 0.1f;

    dc.DrawLine(p2->point, p2->point + Point(a1.x, a1.y));
    dc.DrawLine(p2->point, p2->point + Point(a2.x, a2.y));
}

static void drawPalette(wxDC& dc, const Palette& palette) {
    const int size = 201;
    wxPoint origin(dc.GetSize().x - 50, size + 30);
    wxPen pen = dc.GetPen();
    wxFont font = dc.GetFont();
    font.MakeSmaller();
    dc.SetFont(font);
    for (Size i = 0; i < size; ++i) {
        const float value = palette.getInterpolatedValue(float(i) / (size - 1));
        wxColour color = wxColour(palette(value));
        pen.SetColour(color);
        dc.SetPen(pen);
        dc.DrawLine(wxPoint(origin.x, origin.y - i), wxPoint(origin.x + 30, origin.y - i));
        if (i % 50 == 0) {
            dc.SetPen(*wxWHITE_PEN);
            dc.DrawLine(wxPoint(origin.x, origin.y - i), wxPoint(origin.x + 6, origin.y - i));
            dc.DrawLine(wxPoint(origin.x + 24, origin.y - i), wxPoint(origin.x + 30, origin.y - i));
            dc.SetTextForeground(wxColour(Color::white()));
            // wxFont font(9, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);

            std::wstring text = toPrintableString(value, 1, 1000);
            wxSize extent = dc.GetTextExtent(text);
            drawTextWithSubscripts(dc, text, wxPoint(origin.x - 75, origin.y - i - (extent.y >> 1)));
        }
    }
}

static void drawGrid(wxDC& dc, const ICamera& camera, const float grid) {
    // find (any) direction in the camera plane
    const CameraRay originRay = camera.unproject(Point(0, 0));
    const Vector dir = getNormalized(originRay.target - originRay.origin);
    Vector perpDir;
    if (dir == Vector(0._f, 0._f, 1._f)) {
        perpDir = Vector(1._f, 0._f, 0._f);
    } else {
        perpDir = getNormalized(cross(dir, Vector(0._f, 0._f, 1._f)));
    }

    // find how much is projected grid distance
    const Point shifted = camera.project(originRay.origin + grid * perpDir)->point;
    const float dx = getLength(shifted);
    const float dy = dx;
    const Point origin = camera.project(Vector(0._f))->point;

    wxPen pen(*wxWHITE_PEN);
    pen.SetColour(wxColour(40, 40, 40));
    dc.SetPen(pen);
    for (float x = origin.x; x < dc.GetSize().x; x += dx) {
        dc.DrawLine(wxPoint(x, 0), wxPoint(x, dc.GetSize().y));
    }
    for (float x = origin.x - dx; x >= 0; x -= dx) {
        dc.DrawLine(wxPoint(x, 0), wxPoint(x, dc.GetSize().y));
    }
    for (float y = origin.y; y < dc.GetSize().y; y += dy) {
        dc.DrawLine(wxPoint(0, y), wxPoint(dc.GetSize().x, y));
    }
    for (float y = origin.y - dy; y >= 0; y -= dy) {
        dc.DrawLine(wxPoint(0, y), wxPoint(dc.GetSize().x, y));
    }
}

ParticleRenderer::ParticleRenderer(const GuiSettings& settings) {
    cutoff = settings.get<Float>(GuiSettingsId::ORTHO_CUTOFF);
    grid = settings.get<Float>(GuiSettingsId::VIEW_GRID_SIZE);
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
        const Color color = colorizer.evalColor(i);
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

SharedPtr<wxBitmap> ParticleRenderer::render(const ICamera& camera,
    const RenderParams& params,
    Statistics& stats) const {
    CHECK_FUNCTION(CheckFunction::MAIN_THREAD);
    MEASURE_SCOPE("ParticleRenderer::render");
    const wxSize size(params.size.x, params.size.y);
    SharedPtr<wxBitmap> bitmap = makeShared<wxBitmap>(size, 24);
    wxMemoryDC dc(*bitmap);

    // draw black background (there is no fill method?)
    dc.SetBrush(*wxBLACK_BRUSH);
    dc.DrawRectangle(wxPoint(0, 0), size);

    if (grid > 0.f) {
        drawGrid(dc, camera, grid);
    }

    struct {
        Vector r;
        Vector v;
        bool used = false;
    } dir;

    wxBrush brush(*wxBLACK_BRUSH);
    wxPen pen(*wxBLACK_PEN);

    // draw particles
    for (Size i = 0; i < cached.positions.size(); ++i) {
        if (params.selectedParticle && cached.idxs[i] == params.selectedParticle.value()) {
            // highlight the selected particle
            brush.SetColour(*wxRED);
            pen.SetColour(*wxWHITE);

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
            wxColour color(cached.colors[i]);
            brush.SetColour(color);
            pen.SetColour(color);
        }
        dc.SetBrush(brush);
        dc.SetPen(pen);

        const Optional<ProjectedPoint> p = camera.project(cached.positions[i]);
        ASSERT(p); // cached values must be visible by the camera
        const int size = round(p->radius * params.particles.scale);
        if (size == 0) {
            dc.DrawPoint(p->point);
        } else {
            dc.DrawCircle(p->point, size);
        }
    }
    // after all particles are drawn, draw the velocity vector over
    if (dir.used) {
        drawVector(dc, camera, dir.r, dir.v);
    }

    if (cached.palette) {
        drawPalette(dc, cached.palette.value());
    }
    const Float time = stats.get<Float>(StatisticsId::RUN_TIME);
    dc.DrawText(("t = " + std::to_string(time) + "s").c_str(), wxPoint(0, 0));

    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

NAMESPACE_SPH_END
