#include "gui/objects/Palette.h"
#include "io/Path.h"
#include "objects/utility/StringUtils.h"
#include <fstream>

NAMESPACE_SPH_BEGIN

float Palette::linearToPalette(const float value) const {
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
            palette = log10(value);
        }
        break;
    case PaletteScale::HYBRID:
        if (value > 1.f) {
            palette = 1.f + log10(value);
        } else if (value < -1.f) {
            palette = -1.f - log10(-value);
        } else {
            palette = value;
        }
        break;
    default:
        NOT_IMPLEMENTED;
    }
    ASSERT(isReal(palette), value);
    return palette;
}

float Palette::paletteToLinear(const float value) const {
    switch (scale) {
    case PaletteScale::LINEAR:
        return value;
    case PaletteScale::LOGARITHMIC:
        return exp10(value);
    case PaletteScale::HYBRID:
        if (value > 1.f) {
            return exp10(value - 1.f);
        } else if (value < -1.f) {
            return -exp10(-value - 1.f);
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

Palette::Palette(Array<Point>&& controlPoints, const PaletteScale scale)
    : points(std::move(controlPoints))
    , scale(scale) {
#ifdef SPH_DEBUG
    ASSERT(points.size() >= 2);
    if (scale == PaletteScale::LOGARITHMIC) {
        ASSERT(points[0].value >= 0.f);
    }
    // sanity check, points must be sorted
    for (Size i = 0; i < points.size() - 1; ++i) {
        ASSERT(points[i].value < points[i + 1].value);
    }
#endif
    // save range before converting scale
    range = Interval(points[0].value, points[points.size() - 1].value);
    for (Size i = 0; i < points.size(); ++i) {
        points[i].value = linearToPalette(points[i].value);
    }
}

Interval Palette::getInterval() const {
    ASSERT(points.size() >= 2);
    return range;
}

void Palette::setInterval(const Interval& newRange) {
    Interval oldPaletteRange(points.front().value, points.back().value);
    Interval newPaletteRange(linearToPalette(newRange.lower()), linearToPalette(newRange.upper()));
    const float scale = newPaletteRange.size() / oldPaletteRange.size();
    const float offset = newPaletteRange.lower() - scale * oldPaletteRange.lower();
    for (Size i = 0; i < points.size(); ++i) {
        points[i].value = points[i].value * scale + offset;
    }
    range = newRange;
}

PaletteScale Palette::getScale() const {
    return scale;
}

Rgba Palette::operator()(const float value) const {
    ASSERT(points.size() >= 2);
    ASSERT(scale != PaletteScale::LOGARITHMIC || value >= 0.f);
    const float palette = linearToPalette(value);
    if (palette <= points[0].value) {
        return points[0].color;
    }
    if (palette >= points[points.size() - 1].value) {
        return points[points.size() - 1].color;
    }
    for (Size i = 0; i < points.size() - 1; ++i) {
        if (Interval(points[i].value, points[i + 1].value).contains(palette)) {
            // interpolate
            const float x = (points[i + 1].value - palette) / (points[i + 1].value - points[i].value);
            return points[i].color * x + points[i + 1].color * (1.f - x);
        }
    }
    STOP;
}

float Palette::relativeToPalette(const float value) const {
    ASSERT(value >= 0.f && value <= 1.f);
    const float interpol = points[0].value * (1.f - value) + points.back().value * value;
    switch (scale) {
    case PaletteScale::LINEAR:
        return interpol;
    case PaletteScale::LOGARITHMIC:
        return exp10(interpol);
    case PaletteScale::HYBRID:
        if (interpol > 1.f) {
            return exp10(interpol - 1.f);
        } else if (interpol < -1.f) {
            return -exp10(-interpol - 1.f);
        } else {
            return interpol;
        }
    default:
        NOT_IMPLEMENTED; // in case new scale is added
    }
}

float Palette::paletteToRelative(const float value) const {
    const float linear = linearToPalette(value);
    return (linear - points[0].value) / (points.back().value - points[0].value);
}


Outcome Palette::loadFromFile(const Path& path) {
    try {
        Array<Rgba> colors;
        std::ifstream ifs(path.native());
        std::string line;
        while (std::getline(ifs, line)) {
            std::stringstream ss(replaceAll(line, ",", " "));
            Rgba color;
            while (!ss.eof()) {
                ss >> color.r() >> color.g() >> color.b();
                color.a() = 1.f;
                colors.push(color);
            }
        }
        if (colors.size() < 2) {
            return "No data loaded";
        }

        // preserve the interval of values
        /// \todo improve
        Float from = points.front().value;
        Float to = points.back().value;
        points.resize(colors.size());
        for (Size i = 0; i < points.size(); ++i) {
            points[i].color = colors[i];
            // yes, do not use linearToPalette, we want to map the palette in linear, not on quantities
            points[i].value = from + float(i) * (to - from) / (points.size() - 1);
        }
    } catch (std::exception& e) {
        return std::string("Cannot load palette: ") + e.what();
    }
    return SUCCESS;
}

Outcome Palette::saveToFile(const Path& path, const Size lineCnt) const {
    (void)path;
    (void)lineCnt;
    NOT_IMPLEMENTED;
}

NAMESPACE_SPH_END
