#pragma once

#include "gui/objects/Bitmap.h"
#include "gui/objects/Color.h"
#include "gui/renderers/IRenderer.h"

NAMESPACE_SPH_BEGIN


enum class ColorFlag {
    LINE = 1 << 0,
    FILL = 1 << 1,
    TEXT = 1 << 2,
};

/// \brief Abstraction of a device used for rendering.
///
/// The interface is currently quite minimalistic, can be extended if needed.
class IRenderContext : public Polymorphic {
public:
    /// \brief Returns the size of the canvas associated with the context.
    virtual Pixel size() const = 0;

    /// \brief Selects the color for one or more drawing modes.
    virtual void setColor(const Rgba& color, const Flags<ColorFlag> flags) = 0;

    /// \brief Modifies the thickness of the lines.
    virtual void setThickness(const float thickness) = 0;

    virtual void setFontSize(const int fontSize) = 0;

    /// \brief Fills the whole canvas with given color.
    ///
    /// This erases any previous content.
    virtual void fill(const Rgba& color) = 0;

    /// \brief Draws a line connecting two points.
    virtual void drawLine(const Coords p1, const Coords p2) = 0;

    /// \brief Draws a circle, given its center and a radius.
    virtual void drawCircle(const Coords center, const float radius) = 0;

    /// \brief Draws a triangle given three points.
    virtual void drawTriangle(const Coords p1, const Coords p2, const Coords p3) = 0;

    /// \brief Draws a bitmap, given the position of its left-top corner.
    virtual void drawBitmap(const Coords p, const Bitmap<Rgba>& bitmap) = 0;

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::string& s) = 0;

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::wstring& s) = 0;
};


class PreviewRenderContext : public IRenderContext {
protected:
    Bitmap<Rgba>& bitmap;

    Array<IRenderOutput::Label> labels;

    struct {
        Rgba line = Rgba::black();
        Rgba fill = Rgba::black();
        Rgba text = Rgba::white();
    } colors;

    float thickness = 1._f;

    int fontSize = 9;

public:
    explicit PreviewRenderContext(Bitmap<Rgba>& bitmap)
        : bitmap(bitmap) {}

    Array<IRenderOutput::Label> getLabels() const {
        return labels.clone();
    }

    virtual Pixel size() const override {
        return bitmap.size();
    }

    virtual void setColor(const Rgba& color, const Flags<ColorFlag> flags) override;

    virtual void setThickness(const float newThickness) override;

    virtual void setFontSize(const int newFontSize) override;

    virtual void fill(const Rgba& color) override;

    virtual void drawLine(const Coords p1, const Coords p2) override;

    virtual void drawCircle(const Coords center, const float radius) override;

    virtual void drawTriangle(const Coords p1, const Coords p2, const Coords p3) override;

    virtual void drawBitmap(const Coords p, const Bitmap<Rgba>& subBitmap) override;

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::string& s) override;

    virtual void drawText(const Coords p, const Flags<TextAlign> align, const std::wstring& s) override;

private:
    void drawSafe(const Pixel p, const Rgba c) {
        if (p.x >= 0 && p.y >= 0 && p.x < bitmap.size().x && p.y < bitmap.size().y) {
            bitmap[Pixel(p)] = c;
        }
    }

    void drawSafe(const Coords p, const Rgba c) {
        this->drawSafe(Pixel(p), c);
    }
};

class AntiAliasedRenderContext : public PreviewRenderContext {
public:
    using PreviewRenderContext::PreviewRenderContext;

    virtual void drawCircle(const Coords center, const float radius) override;

private:
    void drawSafe(const Pixel p, const Rgba c) {
        if (p.x >= 0 && p.y >= 0 && p.x < bitmap.size().x && p.y < bitmap.size().y) {
            bitmap[Pixel(p)] = c.over(bitmap[Pixel(p)]);
        }
    }
};

class WxRenderContext : public IRenderContext {
    /// \todo just remember all actions and call them later on main thread
};

NAMESPACE_SPH_END
