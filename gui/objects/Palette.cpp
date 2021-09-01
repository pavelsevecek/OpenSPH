#include "gui/objects/Palette.h"
#include "io/Path.h"
#include "objects/utility/EnumMap.h"
#include "objects/utility/Streams.h"

NAMESPACE_SPH_BEGIN

Palette::Palette(const Palette& other)
    : points(other.points.clone()) {}

Palette& Palette::operator=(const Palette& other) {
    points = copyable(other.points);
    return *this;
}

Palette::Palette(Array<Point>&& controlPoints)
    : points(std::move(controlPoints)) {
    SPH_ASSERT(points.size() >= 2);
    SPH_ASSERT(std::is_sorted(points.begin(), points.end()));
}

Rgba Palette::operator()(const double value) const {
    SPH_ASSERT(points.size() >= 2);
    auto iter = std::lower_bound(
        points.begin(), points.end(), value, [](const Point& p, const double pos) { return p.value < pos; });
    if (iter == points.begin()) {
        return points.front().color;
    } else if (iter == points.end()) {
        return points.back().color;
    } else {
        const Rgba color2 = iter->color;
        const Rgba color1 = (iter - 1)->color;
        const double pos2 = iter->value;
        const double pos1 = (iter - 1)->value;
        const double x = (value - pos1) / (pos2 - pos1);
        return lerp(color1, color2, x);
    }
}

ArrayView<const Palette::Point> Palette::getPoints() const {
    return points;
}

Palette Palette::transform(Function<Rgba(const Rgba&)> func) const {
    Palette cloned = *this;
    for (auto& point : cloned.points) {
        point.color = func(point.color);
    }
    return cloned;
}

bool Palette::empty() const {
    return points.empty();
}

Outcome Palette::loadFromStream(ITextInputStream& ifs) {
    try {
        points.clear();
        String line;
        while (ifs.readLine(line)) {
            line.replaceAll(",", " ");
            std::wstringstream ss(line.toUnicode());
            Rgba color;
            double value;
            while (!ss.eof()) {
                ss >> value >> color.r() >> color.g() >> color.b();
                color.a() = 1.f;
                points.push(Point{ value, color });
            }
        }
        if (points.size() < 2) {
            return makeFailed("No data loaded");
        }
        return SUCCESS;
    } catch (const std::exception& e) {
        return makeFailed("Cannot load palette: {}", exceptionMessage(e));
    }
}

Outcome Palette::loadCsvFromFile(const Path& path) {
    NOT_IMPLEMENTED;
    FileTextInputStream ifs(path);
    return loadFromStream(ifs);
}

Outcome Palette::saveToStream(ITextOutputStream& ofs) const {
    try {
        for (Size i = 0; i < points.size(); ++i) {
            const double value = points[i].value;
            const Rgba color = points[i].color;
            ofs.write(format("{},{},{},{}", value, color.r(), color.g(), color.b()));
            if (i != points.size() - 1) {
                ofs.write(L'\n');
            }
        }
        return SUCCESS;
    } catch (const std::exception& e) {
        return makeFailed("Cannot save palette: {}", exceptionMessage(e));
    }
}

Outcome Palette::saveCsvToFile(const Path& path, const Size lineCnt) const {
    NOT_IMPLEMENTED;
    (void)lineCnt;
    FileTextOutputStream ofs(path);
    return this->saveToStream(ofs);
}

static RegisterEnum<PaletteScale> sPaletteScale({
    { PaletteScale::LINEAR, "linear", "Linear palette" },
    { PaletteScale::LOGARITHMIC, "logarithmic", "Logarithmic palette" },
    { PaletteScale::HYBRID, "hybrid", "Palette linear around zero, logarithmic elsewhere." },
});


Rgba ColorLut::operator()(const double value) const {
    if (scale == PaletteScale::LOGARITHMIC && value <= 0.f) {
        return palette(0);
    }
    return palette(paletteToRelative(value));
}

Palette ColorLut::getPalette() const {
    return palette;
}

void ColorLut::setPalette(const Palette& newPalette) {
    palette = newPalette;
}

Interval ColorLut::getInterval() const {
    return range;
}

void ColorLut::setInterval(const Interval& newRange) {
    range = newRange;
}

PaletteScale ColorLut::getScale() const {
    return scale;
}

void ColorLut::setScale(const PaletteScale& newScale) {
    scale = newScale;
}

double ColorLut::relativeToPalette(const double value) const {
    SPH_ASSERT(value >= 0.f && value <= 1.f);
    const double x1 = paletteToLinear(range.lower());
    const double x2 = paletteToLinear(range.upper());
    const double x = lerp(x1, x2, value);
    return linearToPalette(x);
}

double ColorLut::paletteToRelative(const double value) const {
    const double x1 = paletteToLinear(range.lower());
    const double x2 = paletteToLinear(range.upper());
    const double x = paletteToLinear(value);
    const double rel = (x - x1) / (x2 - x1);
    SPH_ASSERT(isReal(rel), rel);
    return rel;
}

double ColorLut::paletteToLinear(const double value) const {
    double palette;
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
            palette = double(log10(value));
        }
        break;
    case PaletteScale::HYBRID:
        if (value > 1.f) {
            palette = 1.f + double(log10(value));
        } else if (value < -1.f) {
            palette = -1.f - double(log10(-value));
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

double ColorLut::linearToPalette(const double value) const {
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

NAMESPACE_SPH_END
