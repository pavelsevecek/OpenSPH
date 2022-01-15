#pragma once

#include "gui/objects/Color.h"
#include "objects/containers/Array.h"
#include "objects/wrappers/Function.h"
#include "objects/wrappers/Outcome.h"
#include "quantities/QuantityIds.h"

NAMESPACE_SPH_BEGIN

class Path;
struct Pixel;
class ITextInputStream;
class ITextOutputStream;
class IRenderContext;

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
public:
    struct Point {
        float value;
        Rgba color;

        bool operator<(const Point& other) const {
            return value < other.value;
        }
    };

private:
    /// Points in [0, 1] interval
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
    /// The control points must be in interval [0, 1] and must be passed in increasing order. For linear and
    /// hybrid scale, the range can have both positive and negative numbers, for logarithmic scale only
    /// positive numbers (and zero) are allowed.
    Palette(Array<Point>&& controlPoints, const Interval& range, const PaletteScale scale);

    /// \brief Adds a value with given color to the range.
    ///
    /// The value must be contained in the current palette range.
    void addFixedPoint(const float value, const Rgba color);

    /// \brief Returns all points.
    const Array<Point>& getPoints() const;

    /// \brief Returns the interval for which the palette is defined.
    ///
    /// Values outside the interval will be mapped to the colors at the boundary of the interval.
    Interval getInterval() const;

    /// \brief Modifies the value interval.
    void setInterval(const Interval& newRange);

    /// \brief Returns the scale of the palette.
    PaletteScale getScale() const;

    /// \brief Modifies the palette scale.
    void setScale(const PaletteScale& newScale);

    /// \brief Returns the color mapped to given number.
    Rgba operator()(const float value) const;

    /// \brief Returns the palette with colors modified by generic transform.
    Palette transform(Function<Rgba(const Rgba&)> func) const;

    /// \brief Returns a palette with reduced point count.
    Palette subsample(const Size pointCnt) const;

    /// \brief Converts a relative position to an absolute position on a palette.
    ///
    /// The relative position is in interval <0, 1>, the absolute position is given by the control points of
    /// the palette. For linear palette, this function is simply scaling of the interval, for logarithmic and
    /// hybrid palettes it also transforms the input value by the corresponding function.
    float relativeToRange(const float value) const;

    /// \brief Inverse transform to \ref relativeToPalette.
    float rangeToRelative(const float value) const;

    /// \brief Checks if the palette holds some data.
    bool empty() const;

    /// \brief Loads the palette from given input stream.
    Outcome loadFromStream(ITextInputStream& ifs);

    /// \brief Loads the palette from given .csv file.
    Outcome loadFromFile(const Path& path);

    /// \brief Saves the palettes into given output stream.
    Outcome saveToStream(ITextOutputStream& ofs, const Size lineCnt = 256) const;

    /// \brief Saves the palette to a .csv file.
    Outcome saveToFile(const Path& path, const Size lineCnt = 256) const;

private:
    float linearToPalette(const float value) const;

    float paletteToLinear(const float value) const;
};

/// \brief Draws the palette using provided render context
void drawPalette(IRenderContext& context,
    const Pixel origin,
    const Pixel size,
    const Palette& palette,
    const Optional<Rgba>& lineColor);

NAMESPACE_SPH_END
