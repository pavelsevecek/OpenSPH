#pragma once

#include "gui/objects/Color.h"
#include "objects/containers/Array.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

enum class PaletteScale {
    LINEAR,      ///< Control points are interpolated on linear scale
    LOGARITHMIC, ///< Control points are interpolated on logarithmic scale. All points must be positive!
    HYBRID       ///< Logarithic scale for values > 1 and < -1, linear scale on interval [-1, 1]
};

class Palette {
private:
    struct Point {
        float value;
        Color color;
    };
    Array<Point> points;
    Range range;
    PaletteScale scale;


    float linearToPalette(const float value) const;

public:
    Palette() = default;

    Palette(const Palette& other);

    Palette& operator=(const Palette& other);

    /// Create a color palette given control points and their colors. For linear and hybrid scale, controls
    /// points can be both positive or negative numbers, for logarithmic scale only positive numbers (and
    /// zero) are allowed. Control points must be passed in increasing order.
    Palette(Array<Point>&& controlPoints, const PaletteScale scale);

    Range getRange() const;

    Color operator()(const float value) const;

    float getInterpolatedValue(const float value) const;

    /// Default palette for given quantity
    static Palette forQuantity(const QuantityId key, const Range range);
};

NAMESPACE_SPH_END
