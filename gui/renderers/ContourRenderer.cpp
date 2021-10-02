#include "gui/renderers/ContourRenderer.h"
#include "gui/objects/Camera.h"
#include "gui/objects/Colorizer.h"
#include "gui/objects/RenderContext.h"
#include "gui/renderers/ParticleRenderer.h"
#include "objects/containers/UnorderedMap.h"

NAMESPACE_SPH_BEGIN

ContourRenderer::ContourRenderer(SharedPtr<IScheduler> scheduler, const GuiSettings& UNUSED(settings))
    : scheduler(scheduler) {
    kernel = CubicSpline<3>();
    finder = Factory::getFinder(RunSettings::getDefaults());
}

void ContourRenderer::initialize(const Storage& storage,
    const IColorizer& colorizer,
    const ICamera& UNUSED(camera)) {
    cached.positions = storage.getValue<Vector>(QuantityId::POSITION).clone();
    cached.values.resize(cached.positions.size());
    finder->build(*scheduler, cached.positions);

    this->setColorizer(colorizer);
}

bool ContourRenderer::isInitialized() const {
    return !cached.values.empty();
}

void ContourRenderer::setColorizer(const IColorizer& colorizer) {
    parallelFor(*scheduler, 0, cached.positions.size(), [this, &colorizer](const Size i) {
        cached.values[i] = colorizer.evalScalar(i).value();
    });
    cached.palette = colorizer.getPalette();
}

// See https://en.wikipedia.org/wiki/Marching_squares
static FlatMap<Size, Pixel> MS_TABLE(ELEMENTS_UNIQUE,
    {
        // no contour
        { 0b0000, { -1, -1 } },
        { 0b1111, { -1, -1 } },

        // single edge
        { 0b1110, { 2, 3 } },
        { 0b1101, { 1, 2 } },
        { 0b1011, { 0, 1 } },
        { 0b0111, { 3, 0 } },
        { 0b0001, { 2, 3 } },
        { 0b0010, { 1, 2 } },
        { 0b0100, { 0, 1 } },
        { 0b1000, { 3, 0 } },
        { 0b1100, { 1, 3 } },
        { 0b1001, { 0, 2 } },
        { 0b0011, { 1, 3 } },
        { 0b0110, { 0, 2 } },

        /// \todo saddle points
        { 0b1010, { -1, -1 } },
        { 0b0101, { -1, -1 } },
    });

static FlatMap<Size, std::pair<Pixel, Pixel>> MS_EDGE_TO_VTX(ELEMENTS_UNIQUE,
    {
        { 0, { Pixel(0, 0), Pixel(1, 0) } },
        { 1, { Pixel(1, 0), Pixel(1, 1) } },
        { 2, { Pixel(1, 1), Pixel(0, 1) } },
        { 3, { Pixel(0, 1), Pixel(0, 0) } },
    });

static bool isCoordValid(const UnorderedMap<float, Coords>& map, const Coords& p) {
    for (auto& isoAndCoord : map) {
        if (getLength(p - isoAndCoord.value()) < 100) {
            return false;
        }
    }
    return true;
}

void ContourRenderer::render(const RenderParams& params,
    Statistics& UNUSED(stats),
    IRenderOutput& output) const {
    const Pixel size = params.camera->getSize();
    const Optional<CameraRay> ray1 = params.camera->unproject(Coords(0, 0));
    const Optional<CameraRay> ray2 = params.camera->unproject(Coords(size));
    const Vector pos1(ray1->origin[X], ray1->origin[Y], 0._f);
    const Vector pos2(ray2->origin[X], ray2->origin[Y], 0._f);
    const Size resX = params.contours.gridSize;
    const Size resY = Size(resX * float(size.y) / float(size.x));
    const Vector dxdp = Vector(1._f / resX, 1._f / resY, 0._f) * (pos2 - pos1);

    Bitmap<float> grid(Pixel(resX, resY));
    grid.fill(0.f);
    Array<NeighborRecord> neighs;
    for (Size y = 0; y < resY; ++y) {
        for (Size x = 0; x < resX; ++x) {
            const Vector pos = pos1 + dxdp * Vector(x, y, 0);
            const Float h = dxdp[X];
            finder->findAll(pos, 2._f * h, neighs);

            Float sum = 0._f;
            Float weight = 0._f;
            for (const NeighborRecord& n : neighs) {
                const float w = float(kernel.value(pos - cached.positions[n.index], h));
                sum += cached.values[n.index] * w;
                weight += w;
            }
            if (weight > 0._f) {
                grid[Pixel(x, y)] = float(sum / weight);
            }
        }
    }

    Bitmap<Rgba> bitmap(size);
    AntiAliasedRenderContext context(bitmap);
    context.fill(Rgba::black());

    /*context.setColor(Rgba::gray(0.25f), ColorFlag::LINE);
    for (Size x = 0; x < resX; ++x) {
        const int i = x * params.size.x / resX;
        context.drawLine(Coords(i, 0), Coords(i, params.size.y - 1));
    }
    for (Size y = 0; y < resY; ++y) {
        const int i = y * params.size.y / resY;
        context.drawLine(Coords(0, i), Coords(params.size.x - 1, i));
    }*/

    context.setColor(Rgba::white(), ColorFlag::LINE);

    const Coords gridToPixel = Coords(size) / Coords(resX, resY);

    UnorderedMap<float, Coords> labelMap;

    for (Size x = 0; x < resX - 1; ++x) {
        for (Size y = 0; y < resY - 1; ++y) {
            Pixel p(x, y);
            const float v1 = grid[p + Pixel(0, 0)];
            const float v2 = grid[p + Pixel(1, 0)];
            const float v3 = grid[p + Pixel(1, 1)];
            const float v4 = grid[p + Pixel(0, 1)];

            const float vmin = min(v1, v2, v3, v4);
            const float vmax = max(v1, v2, v3, v4);
            const float isoMin = params.contours.isoStep * (int(vmin / params.contours.isoStep) + 1);
            const float isoMax = params.contours.isoStep * (int(vmax / params.contours.isoStep));
            const float step = max(params.contours.isoStep, (isoMax - isoMin) / 3);


            for (float iso = isoMin; iso <= isoMax; iso += step) {
                if (cached.palette) {
                    context.setColor(cached.palette.value()(iso), ColorFlag::LINE);
                }

                const int f1 = int(v1 > iso);
                const int f2 = int(v2 > iso);
                const int f3 = int(v3 > iso);
                const int f4 = int(v4 > iso);

                const Size flag = f1 << 3 | f2 << 2 | f3 << 1 | f4;
                const Pixel edge = MS_TABLE[flag];
                if (edge != Pixel(-1, -1)) {
                    const std::pair<Pixel, Pixel>& ps1 = MS_EDGE_TO_VTX[edge.x];
                    const std::pair<Pixel, Pixel>& ps2 = MS_EDGE_TO_VTX[edge.y];
                    const Pixel p11 = p + ps1.first;
                    const Pixel p12 = p + ps1.second;
                    const Pixel p21 = p + ps2.first;
                    const Pixel p22 = p + ps2.second;

                    SPH_ASSERT((grid[p11] > iso) != (grid[p12] > iso));
                    SPH_ASSERT((grid[p21] > iso) != (grid[p22] > iso));

                    const float rati1 = (grid[p11] - iso) / (grid[p11] - grid[p12]);
                    SPH_ASSERT(rati1 >= 0.f && rati1 <= 1.f, rati1);
                    const Coords c1 = lerp(Coords(p11), Coords(p12), rati1) * gridToPixel;

                    const float rati2 = (grid[p21] - iso) / (grid[p21] - grid[p22]);
                    SPH_ASSERT(rati2 >= 0.f && rati2 <= 1.f, rati2);
                    const Coords c2 = lerp(Coords(p21), Coords(p22), rati2) * gridToPixel;
                    context.drawLine(c1, c2);

                    const Coords labelCoord = (c1 + c2) * 0.5f;
                    if (Optional<Coords&> topmostCoord = labelMap.tryGet(iso)) {
                        if (labelCoord.y < topmostCoord->y && isCoordValid(labelMap, labelCoord)) {
                            topmostCoord.value() = labelCoord;
                        }
                    } else {
                        labelMap.insert(iso, labelCoord);
                    }
                }
            }
        }
    }

    if (params.contours.showLabels) {
        for (const auto& isoAndCoord : labelMap) {
            context.setFontSize(9);
            if (cached.palette) {
                context.setColor(cached.palette.value()(isoAndCoord.key()), ColorFlag::TEXT);
            }
            context.drawText(isoAndCoord.value(), TextAlign::TOP, toString(int(isoAndCoord.key())));
        }
    }

    output.update(bitmap, context.getLabels(), true);
}

NAMESPACE_SPH_END
