#pragma once

#include "gui/Color.h"
#include "objects/containers/Array.h"
#include "quantities/QuantityKey.h"

NAMESPACE_SPH_BEGIN

enum class PaletteScale { LINEAR, LOGARITHMIC, HYBRID };

class Palette : public Object {
private:
    struct Point {
        float value;
        Color color;
    };
    Array<Point> points;

public:
    Palette() = default;

    Palette(Array<Point>&& points)
        : points(std::move(points)) {}

    Color operator()(const float value) {
        ASSERT(points.size() >= 2);
        if (value < points[0].value) {
            return points[0].color;
        }
        if (value > points[points.size() - 1].value) {
            return points[points.size() - 1].color;
        }
        for (int i = 0; i < points.size() - 1; ++i) {
            if (Range(points[i].value, points[i + 1].value).contains(value)) {
                // interpolate
                const float x = (points[i + 1].value - value) / (points[i + 1].value - points[i].value);
                return points[i].color * x + points[i + 1].color * (1.f - x);
            }
        }
    }

    /// Default palette for given quantity
    static Palette forQuantity(const QuantityKey key, const float maxValue) {
        switch (key) {
        case QuantityKey::PRESSURE:
            return Palette({ { 0.f, Color(0.f, 0.f, 0.2f) }, { maxValue, Color(1.f, 0.2f, 0.2f) } });
        case QuantityKey::POSITIONS: // velocity
            return Palette({ { 0.f, Color(0.0f, 0.0f, 0.2f) },
                             { 0.2f * maxValue, Color(0.0f, 0.0f, 1.0f) },
                             { 0.5f * maxValue, Color(1.0f, 0.0f, 0.2f) },
                             { maxValue, Color(1.0f, 1.0f, 0.2f) } });
        case QuantityKey::DAMAGE:
            return Palette({ { 0.f, Color(0.1f, 0.1f, 0.1f) }, { maxValue, Color(0.9f, 0.9f, 0.9f) } });
        default:
            NOT_IMPLEMENTED;
        }
    }
};

NAMESPACE_SPH_END
