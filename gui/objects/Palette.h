#pragma once

#include "gui/objects/Color.h"
#include "objects/containers/Array.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

enum class PaletteScale {
    /// Control points are interpolated on linear scale
    LINEAR,

    /// Control points are interpolated on logarithmic scale. All points must be positive!
    LOGARITHMIC,

    /// Logarithic scale for values > 1 and < -1, linear scale on interval <-1, 1>
    HYBRID
};

/// \brief Represents a color palette, used for mapping arbitrary number to a color.
class Palette {
private:
    struct Point {
        float value;
        Color color;
    };
    Array<Point> points;
    Interval range;
    PaletteScale scale;

public:
    Palette() = default;

    Palette(const Palette& other);

    Palette(Palette&& other) = default;

    Palette& operator=(const Palette& other);

    Palette& operator=(Palette&& other) = default;

    /// \brief Creates a color palette given control points and their colors.
    ///
    /// For linear and hybrid scale, controls points can be both positive or negative numbers, for logarithmic
    /// scale only positive numbers (and zero) are allowed. Control points must be passed in increasing order.
    Palette(Array<Point>&& controlPoints, const PaletteScale scale);

    /// \brief Returns the interval for which the palette is defined.
    ///
    /// Values outside the interval will be mapped to the colors at the boundary of the interval.
    Interval getInterval() const;

    /// \brief Returns the scale of the palette.
    PaletteScale getScale() const;

    /// \brief Returns the color mapped to given number.
    Color operator()(const float value) const;

    /// \brief Converts a relative position to an absolute position on a palette.
    ///
    /// The relative position is in interval <0, 1>, the absolute position is given by the control points of
    /// the palette. For linear palette, this function is simply scaling of the interval, for logarithmic and
    /// hybrid palettes it also transforms the input value by the corresponding function.
    float relativeToPalette(const float value) const;

    /// \brief Inverse transform to \ref relativeToPalette.
    float paletteToRelative(const float value) const;

private:
    float linearToPalette(const float value) const;
};

NAMESPACE_SPH_END
