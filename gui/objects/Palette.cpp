#include "gui/objects/Palette.h"

NAMESPACE_SPH_BEGIN

float Palette::linearToPalette(const float value) const {
    float palette;
    switch (scale) {
    case PaletteScale::LINEAR:
        palette = value;
        break;
    case PaletteScale::LOGARITHMIC:
        palette = log10(value + Eps<float>::value);
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
    ASSERT(isReal(palette));
    return palette;
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
    range = Range(points[0].value, points[points.size() - 1].value);
    for (Size i = 0; i < points.size(); ++i) {
        points[i].value = linearToPalette(points[i].value);
    }
}

Range Palette::getRange() const {
    ASSERT(points.size() >= 2);
    return range;
}

Color Palette::operator()(const float value) const {
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
        if (Range(points[i].value, points[i + 1].value).contains(palette)) {
            // interpolate
            const float x = (points[i + 1].value - palette) / (points[i + 1].value - points[i].value);
            return points[i].color * x + points[i + 1].color * (1.f - x);
        }
    }
    STOP;
}

float Palette::getInterpolatedValue(const float value) const {
    ASSERT(value >= 0.f && value <= 1.f);
    const float interpol = points[0].value * (1.f - value) + points[points.size() - 1].value * value;
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

Palette Palette::forQuantity(const QuantityId key, const Range range) {
    const float x0 = Float(range.lower());
    const float dx = Float(range.size());
    switch (key) {
    case QuantityId::PRESSURE:
        ASSERT(x0 < -1.f);
        return Palette({ { x0, Color(0.3f, 0.3f, 0.8f) },
                           { -1.f, Color(0.f, 0.f, 0.2f) },
                           { 0.f, Color(0.2f, 0.2f, 0.2f) },
                           { 1.f, Color(0.8f, 0.8f, 0.8f) },
                           { x0 + sqrt(dx), Color(1.f, 1.f, 0.2f) },
                           { x0 + dx, Color(0.5f, 0.f, 0.f) } },
            PaletteScale::HYBRID);
    case QuantityId::ENERGY:
        return Palette({ { x0, Color(0.1f, 0.1f, 0.1) },
                           { x0 + 0.001f * dx, Color(0.f, 0.f, 1.f) },
                           { x0 + 0.01f * dx, Color(1.f, 0.f, 0.f) },
                           { x0 + 0.1f * dx, Color(1.0f, 0.6f, 0.4f) },
                           { x0 + dx, Color(1.f, 1.f, 0.f) } },
            PaletteScale::LOGARITHMIC);
    case QuantityId::DEVIATORIC_STRESS:
        return Palette({ { x0, Color(0.f, 0.f, 0.2f) },
                           { x0 + sqrt(dx), Color(1.f, 1.f, 0.2f) },
                           { x0 + dx, Color(0.5f, 0.f, 0.f) } },
            PaletteScale::LOGARITHMIC);
    case QuantityId::DENSITY:
        return Palette({ { x0, Color(0.f, 0.f, 0.2f) },
                           { x0 + 0.5f * dx, Color(1.f, 1.f, 0.2f) },
                           { x0 + dx, Color(0.5f, 0.f, 0.f) } },
            PaletteScale::LINEAR);
    case QuantityId::POSITIONS: // velocity
        return Palette({ { x0, Color(0.5f, 0.5f, 0.5f) },
                           { x0 + 0.001f * dx, Color(0.0f, 0.0f, 0.2f) },
                           { x0 + 0.01f * dx, Color(0.0f, 0.0f, 1.0f) },
                           { x0 + 0.1f * dx, Color(1.0f, 0.0f, 0.2f) },
                           { x0 + dx, Color(1.0f, 1.0f, 0.2f) } },
            PaletteScale::LOGARITHMIC);
    case QuantityId::DAMAGE:
        return Palette(
            { { x0, Color(0.1f, 0.1f, 0.1f) }, { x0 + dx, Color(0.9f, 0.9f, 0.9f) } }, PaletteScale::LINEAR);
    case QuantityId::VELOCITY_DIVERGENCE:
        return Palette({ { x0, Color(0.3f, 0.3f, 0.8f) },
                           { -1e-2f, Color(0.f, 0.f, 0.2f) },
                           { 0.f, Color(0.2f, 0.2f, 0.2f) },
                           { 1e-2f, Color(0.8f, 0.8f, 0.8f) },
                           { x0 + dx, Color(1.0f, 0.6f, 0.f) } },
            PaletteScale::HYBRID);
    default:
        NOT_IMPLEMENTED;
    }
}

NAMESPACE_SPH_END
