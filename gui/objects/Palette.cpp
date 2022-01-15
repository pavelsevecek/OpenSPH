#include "gui/objects/Palette.h"
#include "gui/objects/RenderContext.h"
#include "io/Path.h"
#include "objects/utility/Streams.h"
#include "post/Plot.h"
#include "post/Point.h"

NAMESPACE_SPH_BEGIN

float Palette::paletteToLinear(const float value) const {
    float palette;
    switch (scale) {
    case PaletteScale::LINEAR:
        palette = value;
        break;
    case PaletteScale::LOGARITHMIC:
        // we allow calling this function with zero or negative value, it should simply map to the lowest
        // value on the palette
        if (value < EPS) {
            palette = -LARGE;
        } else {
            palette = float(log10(value));
        }
        break;
    case PaletteScale::HYBRID:
        if (value > 1.f) {
            palette = 1.f + float(log10(value));
        } else if (value < -1.f) {
            palette = -1.f - float(log10(-value));
        } else {
            palette = value;
        }
        break;
    default:
        NOT_IMPLEMENTED;
    }
    SPH_ASSERT(isReal(palette), value);
    return palette;
}

float Palette::linearToPalette(const float value) const {
    switch (scale) {
    case PaletteScale::LINEAR:
        return value;
    case PaletteScale::LOGARITHMIC:
        return double(exp10(value));
    case PaletteScale::HYBRID:
        if (value > 1.f) {
            return double(exp10(value - 1.f));
        } else if (value < -1.f) {
            return double(-exp10(-value - 1.f));
        } else {
            return value;
        }
    default:
        NOT_IMPLEMENTED; // in case new scale is added
    }
}

Palette::Palette(const Palette& other)
    : points(other.points.clone())
    , range(other.range)
    , scale(other.scale) {}

Palette& Palette::operator=(const Palette& other) {
    points = copyable(other.points);
    range = other.range;
    scale = other.scale;
    return *this;
}

Palette::Palette(Array<Point>&& controlPoints, const Interval& range, const PaletteScale scale)
    : points(std::move(controlPoints))
    , range(range)
    , scale(scale) {
#ifdef SPH_DEBUG
    SPH_ASSERT(points.size() >= 2);
    if (scale == PaletteScale::LOGARITHMIC) {
        SPH_ASSERT(range.lower() > 0.f);
    }
    SPH_ASSERT(std::all_of(points.begin(), points.end(), [](const Point& p) { //
        return p.value >= 0 && p.value <= 1;
    }));
    // sanity check, points must be sorted
    SPH_ASSERT(std::is_sorted(points.begin(), points.end()));
#endif
}

void Palette::addFixedPoint(const float value, const Rgba color) {
    /// \todo store separately, do not move in setInterval!
    points.push(Point{ this->rangeToRelative(value), color });
    std::sort(points.begin(), points.end());
}

ArrayView<const Palette::Point> Palette::getPoints() const {
    return points;
}

Interval Palette::getInterval() const {
    SPH_ASSERT(points.size() >= 2);
    return range;
}

void Palette::setInterval(const Interval& newRange) {
    range = newRange;
}

PaletteScale Palette::getScale() const {
    return scale;
}

void Palette::setScale(const PaletteScale& newScale) {
    scale = newScale;
}

Rgba Palette::operator()(const float value) const {
    const float x = rangeToRelative(value);
    SPH_ASSERT(points.size() >= 2);
    auto iter = std::lower_bound(points.begin(), points.end(), x, [](const Point& p, const float pos) { //
        return p.value < pos;
    });
    if (iter == points.begin()) {
        return points.front().color;
    } else if (iter == points.end()) {
        return points.back().color;
    } else {
        const Rgba color2 = iter->color;
        const Rgba color1 = (iter - 1)->color;
        const double pos2 = iter->value;
        const double pos1 = (iter - 1)->value;
        const double f = (x - pos1) / (pos2 - pos1);
        return lerp(color1, color2, f);
    }
}

Palette Palette::transform(Function<Rgba(const Rgba&)> func) const {
    Palette cloned = *this;
    for (auto& point : cloned.points) {
        point.color = func(point.color);
    }
    return cloned;
}

Palette Palette::subsample(const Size pointCnt) const {
    Array<Palette::Point> subsampled;
    for (Size i = 0; i < pointCnt; ++i) {
        const float x = float(i) / (pointCnt - 1);
        const float v = this->relativeToRange(x);
        subsampled.push(Palette::Point{ x, (*this)(v) });
    }
    return Palette(std::move(subsampled), this->getInterval(), this->getScale());
}

float Palette::relativeToRange(const float value) const {
    SPH_ASSERT(value >= 0.f && value <= 1.f);
    const float x1 = paletteToLinear(range.lower());
    const float x2 = paletteToLinear(range.upper());
    const float x = lerp(x1, x2, value);
    return linearToPalette(x);
}

float Palette::rangeToRelative(const float value) const {
    const float x1 = paletteToLinear(range.lower());
    const float x2 = paletteToLinear(range.upper());
    const float x = paletteToLinear(value);
    const float rel = (x - x1) / (x2 - x1);
    SPH_ASSERT(isReal(rel), rel);
    return rel;
}

bool Palette::empty() const {
    return points.empty();
}

Outcome Palette::loadFromStream(ITextInputStream& ifs) {
    try {
        Array<Rgba> colors;
        String line;
        while (ifs.readLine(line)) {
            line.replaceAll(",", " ");
            std::wstringstream ss(line.toUnicode());
            Rgba color;
            while (!ss.eof()) {
                ss >> color.r() >> color.g() >> color.b();
                color.a() = 1.f;
                colors.push(color);
            }
        }
        if (colors.size() < 2) {
            return makeFailed("No data loaded");
        }

        // preserve the interval of values
        /// \todo improve
        float from = points.front().value;
        float to = points.back().value;
        points.resize(colors.size());
        for (Size i = 0; i < points.size(); ++i) {
            points[i].color = colors[i];
            // yes, do not use linearToPalette, we want to map the palette in linear, not on quantities
            points[i].value = from + float(i) * (to - from) / (points.size() - 1);
        }
        return SUCCESS;
    } catch (const std::exception& e) {
        return makeFailed("Cannot load palette: {}", exceptionMessage(e));
    }
}

Outcome Palette::loadFromFile(const Path& path) {
    FileTextInputStream ifs(path);
    return loadFromStream(ifs);
}

Outcome Palette::saveToStream(ITextOutputStream& ofs, const Size lineCnt) const {
    try {
        for (Size i = 0; i < lineCnt; ++i) {
            const float value = this->relativeToRange(float(i) / (lineCnt - 1));
            const Rgba color = this->operator()(value);
            ofs.write(format("{},{},{}", color.r(), color.g(), color.b()));
            if (i != lineCnt - 1) {
                ofs.write(L'\n');
            }
        }
        return SUCCESS;
    } catch (const std::exception& e) {
        return makeFailed("Cannot save palette: {}", exceptionMessage(e));
    }
}

Outcome Palette::saveToFile(const Path& path, const Size lineCnt) const {
    FileTextOutputStream ofs(path);
    return this->saveToStream(ofs, lineCnt);
}

void drawPalette(IRenderContext& context,
    const Pixel origin,
    const Pixel size,
    const Palette& palette,
    const Optional<Rgba>& lineColor) {

    // draw palette
    for (int i = 0; i < size.x; ++i) {
        const float value = palette.relativeToRange(float(i) / (size.x - 1));
        context.setColor(palette(value), ColorFlag::LINE);
        context.drawLine(Coords(origin.x + i, origin.y), Coords(origin.x + i, origin.y + size.y));
    }

    if (lineColor) {
        // draw tics
        const Interval interval = palette.getInterval();
        const PaletteScale scale = palette.getScale();

        Array<Float> tics;
        switch (scale) {
        case PaletteScale::LINEAR:
            tics = getLinearTics(interval, 4);
            break;
        case PaletteScale::LOGARITHMIC: {
            const Float lower = max(interval.lower(), 1.e-6_f);
            const Float upper = interval.upper();
            tics = getLogTics(Interval(lower, upper), 4);
            break;
        }
        case PaletteScale::HYBRID: {
            const Float lower = min(interval.lower(), -2._f);
            const Float upper = max(interval.upper(), 2._f);
            tics = getHybridTics(Interval(lower, upper), 4);
            break;
        }
        default:
            NOT_IMPLEMENTED;
        }
        context.setColor(lineColor.value(), ColorFlag::LINE | ColorFlag::TEXT);
        for (Float tic : tics) {
            const float value = palette.rangeToRelative(float(tic));
            const int i = int(value * size.x);
            context.drawLine(Coords(origin.x + i, origin.y), Coords(origin.x + i, origin.y + 6));
            context.drawLine(
                Coords(origin.x + i, origin.y + size.y - 6), Coords(origin.x + i, origin.y + size.y));

            String text = toPrintableString(tic, 1, 1000);
            context.drawText(Coords(origin.x + i, origin.y + size.y + 15),
                TextAlign::HORIZONTAL_CENTER | TextAlign::VERTICAL_CENTER,
                text);
        }
    }
}


NAMESPACE_SPH_END
