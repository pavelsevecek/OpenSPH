#pragma once

#include "gui/objects/Color.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/Outcome.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

class Path;
class ITextInputStream;
class ITextOutputStream;

/// \brief Represents a color palette, used for mapping arbitrary number to a color.
class Palette {
public:
    struct Point {
        float value;
        Rgba color;

        bool operator<(const Point& other) const {
            return value < other.value;
        }
    };

private:
    Array<Point> points;

public:
    Palette() = default;

    Palette(const Palette& other);

    Palette(Palette&& other) = default;

    Palette& operator=(const Palette& other);

    Palette& operator=(Palette&& other) = default;

    /// \brief Creates a color palette given control points and their colors.
    Palette(Array<Point>&& controlPoints);

    /// \brief Returns the color mapped to given number.
    Rgba operator()(const float value) const;

    /// \brief Returns the control points.
    ArrayView<const Point> getPoints() const;

    /// \brief Returns the palette with colors modified by generic transform.
    Palette transform(Function<Rgba(const Rgba&)> func) const;

    /// \brief Checks if the palette holds some data.
    bool empty() const;

    /// \brief Loads the palette from given input stream.
    Outcome loadFromStream(ITextInputStream& ifs);

    /// \brief Loads the palette from given .csv file.
    Outcome loadCsvFromFile(const Path& path);

    /// \brief Saves the palettes into given output stream.
    Outcome saveToStream(ITextOutputStream& ofs) const;

    /// \brief Saves the palette to a .csv file.
    Outcome saveCsvToFile(const Path& path, const Size lineCnt = 256) const;
};

enum class PaletteScale {
    /// Control points are interpolated on linear scale
    LINEAR,

    /// Control points are interpolated on logarithmic scale. All points must be positive!
    LOGARITHMIC,

    /// Logarithic scale for values > 1 and < -1, linear scale on interval <-1, 1>
    HYBRID
};

class ColorLut {
private:
    Palette palette;
    Interval range;
    PaletteScale scale;

public:
    ColorLut() = default;

    ColorLut(const Palette& palette, const Interval& range, const PaletteScale scale)
        : palette(palette)
        , range(range)
        , scale(scale) {}

    Rgba operator()(const float value) const;

    Palette getPalette() const;

    void setPalette(const Palette& newPalette);

    /// For linear and hybrid scale, controls points can be both positive or negative numbers, for logarithmic
    /// scale only positive numbers (and zero) are allowed. Control points must be passed in increasing order.

    /// \brief Returns the interval for which the palette is defined.
    ///
    /// Values outside the interval will be mapped to the colors at the boundary of the interval.
    Interval getInterval() const;

    void setInterval(const Interval& newRange);

    /// \brief Returns the scale of the palette.
    PaletteScale getScale() const;

    void setScale(const PaletteScale& scale);

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

    float paletteToLinear(const float value) const;
};


NAMESPACE_SPH_END
